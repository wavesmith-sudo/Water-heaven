// Unit tests for the Retro Games logic (qml/games/*.js). Run: node --test tests/games_logic.test.mjs
// The QML JS libraries are loaded into a VM context; `.import "X.js" as X` is resolved here.
import { test } from 'node:test'
import assert from 'node:assert/strict'
import { readFileSync } from 'node:fs'
import vm from 'node:vm'

const dir = new URL('../qml/games/', import.meta.url)
function load(name) {
    const ctx = vm.createContext({ Math })
    let src = readFileSync(new URL(name, dir), 'utf8').replace(/^\.pragma library$/m, '')
    src = src.replace(/^\.import "(\w+)\.js" as (\w+)$/gm, (_, file, alias) => { ctx[alias] = load(file + '.js'); return '' })
    vm.runInContext(src, ctx)
    return ctx
}
const Runner = load('RunnerLogic.js'), Orbit = load('OrbitLogic.js'), Beam = load('BeamLogic.js')

function run(logic, s, seconds, each) {
    const events = []
    for (let t = 0; t < seconds && !s.over; t += 1 / 60) { if (each) each(s); events.push(...logic.step(s, 1 / 60)) }
    return events
}

test('runner: lane changes are clamped to the floor', () => {
    const s = Runner.create(1)
    for (let i = 0; i < 9; ++i) Runner.input(s, 'left')
    assert.equal(s.lane, 0)
    for (let i = 0; i < 9; ++i) Runner.input(s, 'right')
    assert.equal(s.lane, Runner.LANES - 1)
})

test('runner: every row leaves a lane without obstacles', () => {
    const s = Runner.create(7)
    for (let i = 0; i < 500; ++i) {
        s.objs = []; s.dist = i * 3
        Runner.spawnRow(s)
        const blocked = new Set(s.objs.filter(o => o.kind !== 'chip').map(o => o.lane))
        assert.ok(blocked.size < Runner.LANES)
    }
})

test('runner: a barrier is cleared by jumping, a pylon is not', () => {
    const jump = Runner.create(3)
    jump.spawnIn = 99; jump.objs = [{ id: 1, lane: 2, z: 0.05, kind: 'barrier', done: false }]
    Runner.input(jump, 'ok')
    const ev = run(Runner, jump, 0.5)
    assert.ok(ev.includes('jumped')); assert.equal(jump.over, false)

    const pylon = Runner.create(3)
    pylon.spawnIn = 99; pylon.objs = [{ id: 1, lane: 2, z: 0.05, kind: 'pylon', done: false }]
    Runner.input(pylon, 'ok')
    run(Runner, pylon, 0.5)
    assert.equal(pylon.over, true)
})

test('runner: chips score bonus points', () => {
    const s = Runner.create(3)
    s.spawnIn = 99; s.objs = [{ id: 1, lane: 2, z: 0.03, kind: 'chip', done: false }]
    run(Runner, s, 0.2)
    assert.equal(s.chips, 1)
    assert.ok(s.score >= Runner.CHIP_POINTS)
})

test('runner: a player who always takes the open lane survives', () => {
    const s = Runner.create(11)
    run(Runner, s, 60, st => {
        const near = st.objs.filter(o => o.kind !== 'chip' && o.z < 0.25 && o.z > -0.07)
        const blocked = new Set(near.map(o => o.lane))
        if (blocked.has(st.lane)) {
            for (let d = 1; d < Runner.LANES; ++d) {
                for (const l of [st.lane - d, st.lane + d]) if (l >= 0 && l < Runner.LANES && !blocked.has(l)) { st.lane = l; return }
            }
        }
    })
    assert.equal(s.over, false)
    assert.ok(s.score > 500, `score ${s.score}`)
})

test('orbit: ring position wraps and OK swaps colour', () => {
    const s = Orbit.create(1)
    Orbit.input(s, 'left'); assert.equal(s.slot, Orbit.SLOTS - 1)
    Orbit.input(s, 'right'); assert.equal(s.slot, 0)
    Orbit.input(s, 'ok'); assert.equal(s.polarity, 1)
})

test('orbit: matching sparks score with a combo, mismatched ones cost lives', () => {
    const s = Orbit.create(1)
    s.spawnIn = 99
    s.sparks = [{ id: 1, slot: 0, r: 0.99, pol: 0 }]
    assert.deepEqual([...Orbit.step(s, 0.05)], ['catch'])
    s.sparks = [{ id: 2, slot: 0, r: 0.99, pol: 0 }]
    Orbit.step(s, 0.05)
    assert.equal(s.combo, 2); assert.equal(s.score, 30)
    s.sparks = [{ id: 3, slot: 0, r: 0.99, pol: 1 }]
    assert.deepEqual([...Orbit.step(s, 0.05)], ['hit'])
    assert.equal(s.lives, Orbit.LIVES - 1); assert.equal(s.combo, 0)
    // brief guard after a hit
    s.sparks = [{ id: 4, slot: 0, r: 0.99, pol: 1 }]
    Orbit.step(s, 0.05)
    assert.equal(s.lives, Orbit.LIVES - 1)
})

test('orbit: game ends after the last life', () => {
    const s = Orbit.create(5)
    run(Orbit, s, 600, st => { st.polarity = 0; st.sparks.forEach(p => { p.pol = 1; p.slot = st.slot }) })
    assert.equal(s.over, true); assert.equal(s.lives, 0)
})

test('beam: rotation cycles through four orientations', () => {
    let m = Beam.N | Beam.E
    const seen = []
    for (let i = 0; i < 4; ++i) { seen.push(m); m = Beam.rotate(m) }
    assert.deepEqual(seen, [3, 6, 12, 9]); assert.equal(m, 3)
    assert.equal(Beam.rotate(Beam.N | Beam.S), Beam.E | Beam.W)
})

test('beam: generated boards start unsolved and are solvable in par turns', () => {
    for (let seed = 1; seed <= 300; ++seed) {
        const lv = Beam.createLevel(1 + (seed % 8), seed)
        assert.equal(Beam.trace(lv).solved, false, `seed ${seed} starts solved`)
        assert.ok(lv.par > 0)
        let turns = 0
        lv.tiles.forEach((t, i) => {
            if (!t.onPath) return
            while (t.mask !== t.solution) {
                Beam.turn(lv, i % Beam.COLS, Math.floor(i / Beam.COLS)); turns++
            }
        })
        assert.equal(Beam.trace(lv).solved, true, `seed ${seed} unsolved`)
        assert.ok(turns === lv.par, `seed ${seed}: ${turns} > par ${lv.par}`)
    }
})

test('beam: empty tiles cannot be turned and do not count as moves', () => {
    const lv = Beam.createLevel(1, 4)
    const i = lv.tiles.findIndex(t => t.mask === 0)
    if (i >= 0) {
        assert.equal(Beam.turn(lv, i % Beam.COLS, Math.floor(i / Beam.COLS)), false)
        assert.equal(lv.moves, 0)
    }
})
