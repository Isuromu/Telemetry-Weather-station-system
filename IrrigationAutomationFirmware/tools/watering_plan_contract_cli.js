#!/usr/bin/env node
// stdin/stdout bridge keeps JavaScript contract validation canonical for Python adapters.
const fs = require('fs');
const { validatePlan } = require('./watering_plan_contract');

try {
  const request = JSON.parse(fs.readFileSync(0, 'utf8'));
  const result = validatePlan(request.payload, request.options || {});
  process.stdout.write(JSON.stringify(result) + '\n');
} catch (error) {
  process.stdout.write(JSON.stringify({
    ok: false,
    error: { code: 'invalid_validator_request', message: error.message, details: [] },
  }) + '\n');
  process.exitCode = 1;
}
