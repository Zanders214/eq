window.BENCHMARK_DATA = {
  "lastUpdate": 1782410031904,
  "repoUrl": "https://github.com/Zanders214/eq",
  "entries": {
    "ZandersEQ DSP": [
      {
        "commit": {
          "author": {
            "email": "152227414+Zanders214@users.noreply.github.com",
            "name": "Dennis Zanders",
            "username": "Zanders214"
          },
          "committer": {
            "email": "noreply@github.com",
            "name": "GitHub",
            "username": "web-flow"
          },
          "distinct": true,
          "id": "e52abed9f06e03e129cdb4f5c657779159517eec",
          "message": "Merge pull request #13 from Zanders214/ci/perf-rt-safety\n\nci: real-time-safety (RTSan) + performance-benchmark gates",
          "timestamp": "2026-06-25T20:08:25+03:00",
          "tree_id": "53e3d41dd8759b595d7768422d2e0e19c3296532",
          "url": "https://github.com/Zanders214/eq/commit/e52abed9f06e03e129cdb4f5c657779159517eec"
        },
        "date": 1782407516698,
        "tool": "customSmallerIsBetter",
        "benches": [
          {
            "name": "processBlock @48k/512",
            "value": 27063.493,
            "unit": "ns/block"
          },
          {
            "name": "DSP load @48k/512",
            "value": 0.254,
            "unit": "%"
          },
          {
            "name": "processBlock HQ 2x @48k/512",
            "value": 57573.4,
            "unit": "ns/block"
          },
          {
            "name": "DSP load HQ 2x @48k/512",
            "value": 0.54,
            "unit": "%"
          }
        ]
      },
      {
        "commit": {
          "author": {
            "email": "152227414+Zanders214@users.noreply.github.com",
            "name": "Dennis Zanders",
            "username": "Zanders214"
          },
          "committer": {
            "email": "noreply@github.com",
            "name": "GitHub",
            "username": "web-flow"
          },
          "distinct": true,
          "id": "1311d02745e3e1b568c7e82c908e048b7ef6ca20",
          "message": "Merge pull request #14 from Zanders214/perf/cache-eq-coeffs\n\nperf(dsp): cache EQ coefficients (skip steady-state recompute)",
          "timestamp": "2026-06-25T20:48:41+03:00",
          "tree_id": "21a9ebca255dddfe669409f2df33d595d06cd215",
          "url": "https://github.com/Zanders214/eq/commit/1311d02745e3e1b568c7e82c908e048b7ef6ca20"
        },
        "date": 1782410031036,
        "tool": "customSmallerIsBetter",
        "benches": [
          {
            "name": "processBlock @48k/512",
            "value": 15118.814,
            "unit": "ns/block"
          },
          {
            "name": "DSP load @48k/512",
            "value": 0.142,
            "unit": "%"
          },
          {
            "name": "processBlock HQ 2x @48k/512",
            "value": 33027.029,
            "unit": "ns/block"
          },
          {
            "name": "DSP load HQ 2x @48k/512",
            "value": 0.31,
            "unit": "%"
          }
        ]
      }
    ]
  }
}