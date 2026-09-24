// source/tests/benchmark/mandelbrot.apex
// Implementation of Mandelbrot Set Benchmark for Apex language
// https://github.com/is-nobody/apex-lang
// MIT license

import os
import sys

// benchmark: 1000x1000 mandelbrot with 100-iteration cap - tests nested loops and float math
function mandelbrot()
    width = 1000
    height = 1000
    maxIterations = 100

    count = 0

    start = sys.time()

    for py = 0, height - 1
        y0 = (py / height) * 3 - 2

        for px = 0, width - 1
            x0 = (px / width) * 3 - 2

            x = 0
            y = 0
            iteration = 0

            for x * x + y * y <= 4 and iteration < maxIterations
                xtemp = x * x - y * y + x0
                y = 2 * x * y + y0
                x = xtemp
                iteration = iteration + 1

            if iteration == maxIterations
                count = count + 1

    elapsed = sys.time() - start
    os.output("Mandelbrot ({width}x{height}): points in set = {count}, elapsed {elapsed}")

// execute benchmark
mandelbrot()