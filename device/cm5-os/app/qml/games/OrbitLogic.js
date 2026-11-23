.pragma library
.import "Rng.js" as Rng
// Orbit Shift: sparks fly out from the core along sixteen spokes. Move around the ring
// to meet them. Catch sparks that match your colour (OK swaps cyan/magenta); a spark of
// the other colour costs a life. Catching in a row builds a combo multiplier.

var SLOTS = 16
var LIVES = 3

function create(seed) {
    return { slot: 0, polarity: 0, sparks: [], lives: LIVES, score: 0, combo: 0, best: 0,
             time: 0, spawnIn: 1.0, guard: 0, over: false, rand: Rng.make(seed), nextId: 1 }
}

function input(s, action) {
    if (s.over) return
    if (action === "left" || action === "down") s.slot = (s.slot + SLOTS - 1) % SLOTS
    else if (action === "right" || action === "up") s.slot = (s.slot + 1) % SLOTS
    else if (action === "ok") s.polarity = 1 - s.polarity
}

function speed(s) { return Math.min(0.75, 0.3 + s.time * 0.006) }

// Spawn on a spoke that is reachable: no further from the player than the spark's travel allows.
function spawn(s) {
    var reach = Math.max(2, Math.min(SLOTS / 2, Math.floor(1 / speed(s) * 3)))
    var offset = Rng.int(s.rand, reach * 2 + 1) - reach
    s.sparks.push({ id: s.nextId++, slot: (s.slot + offset + SLOTS) % SLOTS, r: 0,
                    pol: Rng.int(s.rand, 2) })
}

// Returns a list of events ("catch", "hit", "miss").
function step(s, dt) {
    var events = []
    if (s.over) return events
    dt = Math.min(dt, 0.1)
    s.time += dt
    s.guard = Math.max(0, s.guard - dt)
    var v = speed(s)
    for (var i = 0; i < s.sparks.length; ++i) {
        var p = s.sparks[i]
        p.r += v * dt
        if (p.r < 1 || p.done) continue
        p.done = true
        if (p.slot !== s.slot) {
            if (p.pol === s.polarity) { s.combo = 0; events.push("miss") }
        } else if (p.pol === s.polarity) {
            s.combo++
            s.best = Math.max(s.best, s.combo)
            s.score += 10 * Math.min(8, s.combo)
            events.push("catch")
        } else if (s.guard <= 0) {
            s.lives--
            s.combo = 0
            s.guard = 1.0
            events.push("hit")
            if (s.lives <= 0) s.over = true
        }
    }
    s.sparks = s.sparks.filter(function(p) { return !p.done })
    s.spawnIn -= dt
    if (s.spawnIn <= 0) {
        spawn(s)
        s.spawnIn = Math.max(0.38, 1.1 - s.time * 0.012)
    }
    return events
}
