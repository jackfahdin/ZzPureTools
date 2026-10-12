import csv
import json
import math
import sys

rows = list(csv.DictReader(open(sys.argv[1])))
groups = {}
for row in rows:
    row['ns'] = int(row['ns'])
    row['value'] = int(row['value'])
    groups.setdefault(int(row['iteration']), []).append(row)

def stats(values):
    values = sorted(values)
    return {'n': len(values), 'p50': values[math.ceil(len(values)*.5)-1],
            'p95': values[math.ceil(len(values)*.95)-1], 'max': max(values)} if values else {}

frames = []
costs = {name: [] for name in ('paint', 'update', 'timer', 'xcb-sync')}
for iteration, events in groups.items():
    timer = None
    previous = None
    update = None
    sync = 0
    starts = {}
    for e in events:
        t, kind = e['ns']/1e6, e['event']
        if kind == 'timer-start': timer = t
        if kind == 'update-start':
            update = t
            sync = 0
        for name in costs:
            if kind == name+'-start': starts[name] = t
            if kind == name+'-end' and name in starts: costs[name].append(t-starts[name])
        if kind == 'xcb-sync-end': sync += t-starts['xcb-sync']
        if kind == 'paint-start':
            if timer is not None and previous is not None and previous[1] is not None:
                pt, prevtimer, prevsync = previous
                frames.append({'iteration': iteration, 'at': t, 'frame': t-pt,
                               'timer': timer-prevtimer, 'lag': t-timer,
                               'lagDelta': (t-timer)-(pt-prevtimer),
                               'sync': sync, 'syncDelta': sync-prevsync,
                               'updateToPaint': t-update if update is not None else None})
            previous = t, timer, sync

slow = [f for f in frames if f['frame'] > 16.7]
print(json.dumps({'costs': {k: stats(v) for k, v in costs.items()},
    'frame': stats([f['frame'] for f in frames]),
    'slowFrames': len(slow),
    'slowTimerOverBudget': sum(f['timer'] > 16.7 for f in slow),
    'slowTimer': stats([f['timer'] for f in slow]),
    'slowLag': stats([f['lag'] for f in slow]),
    'slowLagDelta': stats([f['lagDelta'] for f in slow]),
    'slowSync': stats([f['sync'] for f in slow]),
    'slowFramesWithSyncOver1ms': sum(f['sync'] > 1 for f in slow),
    'worstFrames': sorted(frames, key=lambda f: f['frame'], reverse=True)[:10]}, indent=2))
