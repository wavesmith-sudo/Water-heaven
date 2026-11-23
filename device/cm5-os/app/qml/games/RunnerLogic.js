.pragma library
.import "Rng.js" as Rng
// Grid Runner: steer across five lanes of the neon floor. Low barriers can be jumped
// with OK; tall pylons must be dodged. Cyan chips are worth bonus points.
// Depth z runs from 1 (horizon) to 0 (the player's line).

var LANES = 5
var JUMP_TIME = 0.7
var CHIP_POINTS = 25

function create(seed) {
    return { lane: 2, jump: 0, objs: [], speed: 0.4, dist: 0, chips: 0, score: 0,
             over: false, spawnIn: 0.6, rand: Rng.make(seed), nextId: 1 }
}

function airborne(s) { return s.jump > 0 }

function input(s, action) {
    if (s.over) return
    if (action === "left") s.lane = Math.max(0, s.lane - 1)
    else if (action === "right") s.lane = Math.min(LANES - 1, s.lane + 1)
    else if ((action === "ok" || action === "up") && !airborne(s)) s.jump = JUMP_TIME
}

// One row of objects. At least one lane always stays free of pylons, so every row is passable.
function spawnRow(s) {
    var open = Rng.int(s.rand, LANES)
    var hazards = 1 + Rng.int(s.rand, Math.min(3, 1 + Math.floor(s.dist / 150)))
    var used = {}
    used[open] = true
    for (var i = 0; i < hazards; ++i) {
        var lane = Rng.int(s.rand, LANES)
        if (used[lane]) continue
        used[lane] = true
        s.objs.push({ id: s.nextId++, lane: lane, z: 1, kind: s.rand() < 0.45 ? "pylon" : "barrier", done: false })
    }
    if (s.rand() < 0.6)
        s.objs.push({ id: s.nextId++, lane: open, z: 1, kind: "chip", done: false })
}

// Advance by dt seconds. Returns a list of events ("chip", "jumped", "crash").
function step(s, dt) {
    var events = []
    if (s.over) return events
    dt = Math.min(dt, 0.1)
    s.speed = Math.min(1.15, 0.4 + s.dist * 0.0012)
    s.dist += s.speed * dt * 40
    s.jump = Math.max(0, s.jump - dt)
    for (var i = 0; i < s.objs.length; ++i) {
        var o = s.objs[i]
        o.z -= s.speed * dt
        if (o.done || o.z > 0.02 || o.z < -0.06 || o.lane !== s.lane) continue
        if (o.kind === "chip") { o.done = true; s.chips++; events.push("chip") }
        else if (o.kind === "barrier" && airborne(s)) { o.done = true; events.push("jumped") }
        else { o.done = true; s.over = true; events.push("crash") }
    }
    s.objs = s.objs.filter(function(o) { return o.z > -0.15 && !(o.done && o.kind === "chip") })
    s.spawnIn -= dt
    if (s.spawnIn <= 0) {
        spawnRow(s)
        s.spawnIn = Math.max(0.42, 1.05 - s.dist * 0.0008) / (s.speed / 0.4) * 0.9
    }
    s.score = Math.floor(s.dist) + s.chips * CHIP_POINTS
    return events
}
