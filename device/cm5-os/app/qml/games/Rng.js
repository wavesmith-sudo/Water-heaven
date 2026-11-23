.pragma library
// Small seeded PRNG so game logic is deterministic in tests.
function make(seed) {
    var s = (seed >>> 0) || 1
    return function() { s = (Math.imul(s, 1664525) + 1013904223) >>> 0; return s / 4294967296 }
}
function int(rand, n) { return Math.floor(rand() * n) }
