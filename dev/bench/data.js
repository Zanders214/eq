window.BENCHMARK_DATA = {
  "lastUpdate": 1782500537502,
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
          "id": "f23162adaad2e19959d4d771873ba6dd3c6bd8f8",
          "message": "Merge pull request #15 from Zanders214/claude/proq4-dsp-shapes-atv07d\n\nfeat(dsp): Pro-Q 4 filter shapes (tilt/band-pass/all-pass) + steeper slopes",
          "timestamp": "2026-06-26T19:32:47+03:00",
          "tree_id": "775eddf48e0783762719a9556871287aefd6cd05",
          "url": "https://github.com/Zanders214/eq/commit/f23162adaad2e19959d4d771873ba6dd3c6bd8f8"
        },
        "date": 1782491709295,
        "tool": "customSmallerIsBetter",
        "benches": [
          {
            "name": "processBlock @48k/512",
            "value": 23873.36,
            "unit": "ns/block"
          },
          {
            "name": "DSP load @48k/512",
            "value": 0.224,
            "unit": "%"
          },
          {
            "name": "processBlock HQ 2x @48k/512",
            "value": 51357.957,
            "unit": "ns/block"
          },
          {
            "name": "DSP load HQ 2x @48k/512",
            "value": 0.481,
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
          "id": "9628ef173d9f2f45982fe46cd2e8b8148744eebc",
          "message": "Merge pull request #17 from Zanders214/claude/proq4-band-panel-a9vmj8\n\nfeat(gui): repurpose BandEditorRail into a Pro-Q-4 floating band panel (Branch 4)",
          "timestamp": "2026-06-26T19:19:41+02:00",
          "tree_id": "00245fd65dd9f27daf299689acdde082ad81f972",
          "url": "https://github.com/Zanders214/eq/commit/9628ef173d9f2f45982fe46cd2e8b8148744eebc"
        },
        "date": 1782494517504,
        "tool": "customSmallerIsBetter",
        "benches": [
          {
            "name": "processBlock @48k/512",
            "value": 24558.544,
            "unit": "ns/block"
          },
          {
            "name": "DSP load @48k/512",
            "value": 0.23,
            "unit": "%"
          },
          {
            "name": "processBlock HQ 2x @48k/512",
            "value": 53755.437,
            "unit": "ns/block"
          },
          {
            "name": "DSP load HQ 2x @48k/512",
            "value": 0.504,
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
          "id": "66c06fdc497710ae035b3b7946088a2a969f6e02",
          "message": "feat(engine): 6→24 dynamic band pool + new parameter surface (Pro-Q 4 foundation) (#16)\n\nFoundation branch: dynamic 24-slot band pool (Pro-Q add/remove model), per-band `active` flag gating the audio path, dynamic-band API (addBand/removeBand/activeBandCount/firstFreeSlot/isBandActive), second pre-EQ analyzer tap, new global params (gainScale/analyzerOn/analyzerRange/globalBypass/phaseMode), and the forEachParamId fix capturing dynDir + active for undo/A-B/presets. Old 6-band states load to exactly 6 active bands. MatchFit capped to its 6-band solver. Verified: ctest, pluginval strictness 10, RTSan, SonarCloud coverage all green.",
          "timestamp": "2026-06-26T19:24:13+02:00",
          "tree_id": "20abb928e7ad1f657fa41f2c1b005d7925fda526",
          "url": "https://github.com/Zanders214/eq/commit/66c06fdc497710ae035b3b7946088a2a969f6e02"
        },
        "date": 1782494817918,
        "tool": "customSmallerIsBetter",
        "benches": [
          {
            "name": "processBlock @48k/512",
            "value": 31023.616,
            "unit": "ns/block"
          },
          {
            "name": "DSP load @48k/512",
            "value": 0.291,
            "unit": "%"
          },
          {
            "name": "processBlock HQ 2x @48k/512",
            "value": 64342.707,
            "unit": "ns/block"
          },
          {
            "name": "DSP load HQ 2x @48k/512",
            "value": 0.603,
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
          "id": "f89300056a6aa02642471d04e9d1c83051718d29",
          "message": "Merge pull request #18 from Zanders214/claude/proq4-hero-graph-d8l2i2\n\nfeat(graph): Pro-Q 4 hero display — dynamic bands, pre/post spectrum, C4 seam",
          "timestamp": "2026-06-26T20:39:27+02:00",
          "tree_id": "9d93b0a873dca8a273af8138243d12e3818d8854",
          "url": "https://github.com/Zanders214/eq/commit/f89300056a6aa02642471d04e9d1c83051718d29"
        },
        "date": 1782499316502,
        "tool": "customSmallerIsBetter",
        "benches": [
          {
            "name": "processBlock @48k/512",
            "value": 30874.932,
            "unit": "ns/block"
          },
          {
            "name": "DSP load @48k/512",
            "value": 0.289,
            "unit": "%"
          },
          {
            "name": "processBlock HQ 2x @48k/512",
            "value": 64447.753,
            "unit": "ns/block"
          },
          {
            "name": "DSP load HQ 2x @48k/512",
            "value": 0.604,
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
          "id": "3388e91eb75d8e4d5390048fbc1ac068391830d1",
          "message": "Merge pull request #19 from Zanders214/claude/zanderseq-layout-refactor-vkt1i1\n\nfeat(layout): Pro-Q 4 full-width reflow, neon bottom toolbar & floating band panel",
          "timestamp": "2026-06-26T20:59:41+02:00",
          "tree_id": "5b00767b026953e7a754bd3db02b031d6d2bda4d",
          "url": "https://github.com/Zanders214/eq/commit/3388e91eb75d8e4d5390048fbc1ac068391830d1"
        },
        "date": 1782500536607,
        "tool": "customSmallerIsBetter",
        "benches": [
          {
            "name": "processBlock @48k/512",
            "value": 30853.97,
            "unit": "ns/block"
          },
          {
            "name": "DSP load @48k/512",
            "value": 0.289,
            "unit": "%"
          },
          {
            "name": "processBlock HQ 2x @48k/512",
            "value": 64502.974,
            "unit": "ns/block"
          },
          {
            "name": "DSP load HQ 2x @48k/512",
            "value": 0.605,
            "unit": "%"
          }
        ]
      }
    ]
  }
}