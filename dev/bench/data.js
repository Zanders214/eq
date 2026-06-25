window.BENCHMARK_DATA = {
  "lastUpdate": 1782407517618,
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
      }
    ]
  }
}