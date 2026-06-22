import http from 'k6/http';
import { sleep } from 'k6';
import { SharedArray } from 'k6/data';
import { Trend, Rate } from 'k6/metrics';

const BASE = __ENV.BASE_URL || 'http://127.0.0.1:10002';
const SCENARIO = __ENV.SCENARIO || 'mixed';
const NO_CONNECTION_REUSE = (__ENV.NO_CONNECTION_REUSE || 'false') === 'true';

const tokens = new SharedArray('tokens', function () {
  if (SCENARIO === 'health') {
    return [];
  }
  const data = JSON.parse(open('./users.json'));
  return Array.isArray(data) ? data.map(u => u.access_token) : data.users.map(u => u.access_token);
});

const infoTrend = new Trend('info_duration');
const healthTrend = new Trend('health_duration');
const failRate = new Rate('request_failed');

export const options = {
  stages: [
    { duration: '10s', target: 10 },
    { duration: '30s', target: 50 },
    { duration: '30s', target: 100 },
    { duration: '30s', target: 200 },
    { duration: '30s', target: 500 },
    { duration: '20s', target: 0 },
  ],
  thresholds: {
    http_req_duration: ['p(95)<500', 'p(99)<2000'],
    request_failed: ['rate<0.01'],
  },
  noConnectionReuse: NO_CONNECTION_REUSE,
};

function recordResult(res) {
  failRate.add(res.status !== 200);
}

function requestHealth() {
  const r = http.get(`${BASE}/api/v1/health`);
  healthTrend.add(r.timings.duration);
  recordResult(r);
}

function requestInfo() {
  const token = tokens[__VU % tokens.length];
  const r = http.get(`${BASE}/api/v1/auth/info`, {
    headers: { Authorization: `Bearer ${token}` },
  });
  infoTrend.add(r.timings.duration);
  recordResult(r);
}

export default function () {
  if (SCENARIO === 'health') {
    requestHealth();
  } else if (SCENARIO === 'info') {
    requestInfo();
  } else {
    requestHealth();
    requestInfo();
  }


  sleep(1);
}
