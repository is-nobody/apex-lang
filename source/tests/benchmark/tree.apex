import os

function tree_sum(n)
    if n == 0
        return 0
    return 1 + tree_sum(n - 1) + tree_sum(n - 1)

function tree20()
    start = os.time()
    result = tree_sum(20)
    elapsed = os.time() - start
    os.output("tree(20) = {result}, excepted 1048575")
    os.output("Time: {elapsed} seconds")

function tree25()
    start = os.time()
    result = tree_sum(25)
    elapsed = os.time() - start
    os.output("tree(25) = {result}, excepted 33554431")
    os.output("Time: {elapsed} seconds")

function tree30()
    start = os.time()
    result = tree_sum(30)
    elapsed = os.time() - start
    os.output("tree(30) = {result}, excepted 1073741823")
    os.output("Time: {elapsed} seconds")

tree20()
tree25()
tree30()