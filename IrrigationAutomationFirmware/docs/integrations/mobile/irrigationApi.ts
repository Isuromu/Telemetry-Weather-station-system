/** Typed React Native client for the temporary Pi API and future local server. */

export type IrrigationConnectionProfile = {
  baseUrl: string;
  apiVersion: "v1";
  getAccessToken: () => Promise<string | null>;
};

export type WateringPlanRequest = {
  schema_version: 1;
  plan_id: string;
  revision: number;
  status: "scheduled" | "cancelled";
  source: "human" | "ai";
  plan_group_id?: string | null;
  summary?: string | null;
  created_at: string;
  timezone: string;
  start_at: string;
  valid_until: string;
  targets: { mode: "selected"; field_ids: string[] } | { mode: "all" };
  run_mode: "parallel";
  stop:
    | { type: "duration"; value: number; unit: "seconds" | "minutes" | "hours" }
    | { type: "volume"; value: number; unit: "m3" | "liters" };
  hard_max_runtime_sec: number;
};

export type ApiSuccess<T> = { api_version: "v1"; request_id: string; data: T };
export type ApiFailure = {
  api_version: "v1";
  request_id: string;
  error: { code: string; message: string; details: unknown[] };
};
export type WateringPlanRecord = WateringPlanRequest & {
  state: string;
  accepted_at: string;
  updated_at: string;
  delay_reason?: string;
  execution?: Record<string, unknown>;
};
export type WateringPlanCollection = {
  items: WateringPlanRecord[];
  next_cursor: string | null;
};
export type DeviceStateSnapshot = {
  generated_at: string;
  status?: "available" | "stale" | "unavailable";
  [key: string]: unknown;
};

export class IrrigationApiError extends Error {
  constructor(
    readonly status: number,
    readonly code: string,
    message: string,
    readonly details: unknown[] = [],
    readonly requestId?: string,
  ) {
    super(message);
    this.name = "IrrigationApiError";
  }
}

type FetchLike = (input: string, init?: RequestInit) => Promise<Response>;
type RequestIdFactory = () => string;

function makeRequestId(): string {
  // Keep UUID generation injectable because React Native runtimes differ in
  // their support for crypto.randomUUID(). This is a diagnostic id, not a
  // credential or an idempotency key.
  return "xxxxxxxx-xxxx-4xxx-yxxx-xxxxxxxxxxxx".replace(/[xy]/g, character => {
    const random = Math.floor(Math.random() * 16);
    return (character === "x" ? random : (random & 0x3) | 0x8).toString(16);
  });
}

export class IrrigationApiClient {
  private readonly base: string;

  constructor(
    private readonly profile: IrrigationConnectionProfile,
    private readonly fetcher: FetchLike = fetch,
    private readonly newRequestId: RequestIdFactory = makeRequestId,
  ) {
    if (profile.apiVersion !== "v1") throw new Error("Only irrigation API v1 is supported");
    this.base = `${profile.baseUrl.replace(/\/+$/, "")}/api/${profile.apiVersion}`;
  }

  health(): Promise<ApiSuccess<{ time: string; clock_synchronized: boolean }>> {
    return this.request("GET", "/health");
  }

  deviceState(): Promise<ApiSuccess<DeviceStateSnapshot>> {
    return this.request("GET", "/device-state");
  }

  getPlan(planId: string): Promise<ApiSuccess<WateringPlanRecord>> {
    return this.request("GET", `/watering-plans/${encodeURIComponent(planId)}`);
  }

  putPlan(plan: WateringPlanRequest): Promise<ApiSuccess<WateringPlanRecord>> {
    // Callers retain this exact plan object after a timeout and retry it with
    // the same plan_id and revision. This method never invents a new identity.
    return this.request("PUT", `/watering-plans/${encodeURIComponent(plan.plan_id)}`, plan);
  }

  cancelPlan(planId: string): Promise<ApiSuccess<WateringPlanRecord>> {
    return this.request("POST", `/watering-plans/${encodeURIComponent(planId)}/cancel`, {});
  }

  listPlans(options: {
    from?: string;
    to?: string;
    limit?: number;
    cursor?: string;
  } = {}): Promise<ApiSuccess<WateringPlanCollection>> {
    const query = new URLSearchParams();
    if (options.from) query.set("from", options.from);
    if (options.to) query.set("to", options.to);
    if (options.limit !== undefined) query.set("limit", String(options.limit));
    if (options.cursor) query.set("cursor", options.cursor);
    const suffix = query.toString();
    return this.request("GET", `/watering-plans${suffix ? `?${suffix}` : ""}`);
  }

  private async request<T>(method: string, path: string, body?: unknown): Promise<ApiSuccess<T>> {
    const token = await this.profile.getAccessToken();
    if (!token) throw new IrrigationApiError(401, "credentials_missing", "Pair with the irrigation gateway first.");
    const requestId = this.newRequestId();
    const response = await this.fetcher(this.base + path, {
      method,
      headers: {
        Accept: "application/json",
        Authorization: `Bearer ${token}`,
        "Content-Type": "application/json",
        "X-Request-ID": requestId,
      },
      ...(body === undefined ? {} : { body: JSON.stringify(body) }),
    });

    let payload: ApiSuccess<T> | ApiFailure;
    try {
      payload = (await response.json()) as ApiSuccess<T> | ApiFailure;
    } catch {
      throw new IrrigationApiError(response.status, "invalid_response", "The gateway returned an unreadable response.", [], requestId);
    }
    if (!response.ok || "error" in payload) {
      const failure = "error" in payload ? payload : undefined;
      throw new IrrigationApiError(
        response.status,
        failure?.error.code ?? "http_error",
        failure?.error.message ?? `The gateway request failed (${response.status}).`,
        failure?.error.details ?? [],
        failure?.request_id ?? requestId,
      );
    }
    if (payload.api_version !== "v1" || !payload.request_id || !("data" in payload)) {
      throw new IrrigationApiError(response.status, "invalid_response", "The gateway response does not match API v1.", [], requestId);
    }
    return payload;
  }
}
