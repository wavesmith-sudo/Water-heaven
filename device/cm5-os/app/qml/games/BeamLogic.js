.pragma library
.import "Rng.js" as Rng
// Beam Router: rotate mirror tiles so the laser crosses the board from the emitter on the
// left to the receiver on the right. Each tile joins two sides; OK turns it a quarter clockwise.
// Sides are bits: N=1, E=2, S=4, W=8.

var N = 1, E = 2, S = 4, W = 8
var COLS = 8, ROWS = 5
var SHAPES = [N | S, E | W, N | E, E | S, S | W, W | N]

function rotate(mask) { return ((mask << 1) | (mask >> 3)) & 15 }
function opposite(side) { return side === N ? S : side === S ? N : side === E ? W : E }

function turnsBetween(from, to) {
    for (var k = 0; k < 4; ++k) { if (from === to) return k; from = rotate(from) }
    return -1
}

// A guaranteed route: in each column, enter from the west, run up or down, then leave east.
function createLevel(level, seed) {
    var rand = Rng.make(seed)
    var tiles = []
    for (var i = 0; i < COLS * ROWS; ++i) tiles.push({ mask: 0, solution: 0, onPath: false })
    var startRow = Rng.int(rand, ROWS), row = startRow
    var bendiness = Math.min(0.85, 0.35 + level * 0.08)
    for (var x = 0; x < COLS; ++x) {
        var target = rand() < bendiness ? Rng.int(rand, ROWS) : row
        var dir = target > row ? S : N
        var from = W
        while (row !== target) {
            var t = tiles[row * COLS + x]
            t.solution = from | dir; t.onPath = true
            from = opposite(dir)
            row += dir === S ? 1 : -1
        }
        tiles[row * COLS + x].solution = from | E
        tiles[row * COLS + x].onPath = true
    }
    var endRow = row
    for (var j = 0; j < tiles.length; ++j) {
        var tile = tiles[j]
        if (!tile.onPath) tile.solution = rand() < 0.12 ? 0 : SHAPES[Rng.int(rand, SHAPES.length)]
        tile.mask = tile.solution
        var turns = Rng.int(rand, 4)
        for (var k = 0; k < turns; ++k) tile.mask = rotate(tile.mask)
    }
    var lv = { level: level, tiles: tiles, startRow: startRow, endRow: endRow, par: 0, moves: 0 }
    if (trace(lv).solved) { // never hand out a finished board
        var first = tiles[startRow * COLS]
        first.mask = rotate(first.mask)
    }
    lv.par = minimumTurns(lv)
    return lv
}

// Fewest quarter turns that solve the board along the generated route.
function minimumTurns(lv) {
    var total = 0
    for (var i = 0; i < lv.tiles.length; ++i) {
        var t = lv.tiles[i]
        if (!t.onPath) continue
        var k = turnsBetween(t.mask, t.solution)
        var straight = t.solution === (N | S) || t.solution === (E | W)
        total += straight ? k % 2 : k
    }
    return total
}

function turn(lv, col, row) {
    var t = lv.tiles[row * COLS + col]
    if (!t || t.mask === 0) return false
    t.mask = rotate(t.mask)
    lv.moves++
    return true
}

// Follow the beam. Returns the lit tile indices and whether it reached the receiver.
function trace(lv) {
    var lit = [], x = 0, y = lv.startRow, entry = W
    for (var guard = 0; guard <= COLS * ROWS; ++guard) {
        if (x < 0 || y < 0 || x >= COLS || y >= ROWS) return { lit: lit, solved: false }
        var t = lv.tiles[y * COLS + x]
        if (!(t.mask & entry)) return { lit: lit, solved: false }
        lit.push(y * COLS + x)
        var exit = t.mask & ~entry
        if (exit === E && x === COLS - 1) return { lit: lit, solved: y === lv.endRow }
        if (exit === N) y--
        else if (exit === S) y++
        else if (exit === E) x++
        else if (exit === W) x--
        else return { lit: lit, solved: false }
        entry = opposite(exit)
    }
    return { lit: lit, solved: false }
}

function levelScore(lv) {
    return 100 + lv.level * 50 + Math.max(0, lv.par + 4 - lv.moves) * 20
}
