#!/usr/bin/python3

import sys

def build(tree, states, code):
  if code in states:
    return states[code][0]
  index = len(tree)
  tree.append('')	# Reserve a node.
  n0 = build(tree, states, code + '0')
  n1 = build(tree, states, code + '1')
  tree[index] = (n0, n1)
  return index

states = {}
for line in sys.stdin:
  code, v0, v1 = line.strip().split('\t')
  assert code not in states
  v0 = int(v0)
  v1 = int(v1)
  if v0 == 254 and v1 >= 64:
    assert v1 % 64 == 0
    v1 //= 64
  index = len(states) + 1
  states[code] = (index, (v0, v1))

tree = [None] * len(states)	# Reserve the space for the leaf nodes.
build(tree, states, '')
tree[0] = tree[len(states)]	# Move the initial state to the top.
for num, v in states.values():
  tree[num] = v

print('const uint16_t fsm_[%d] = {\n ' % len(tree), end='')
for i, (v0, v1) in enumerate(tree):
  if i != 0 and i % 8 == 0:
    print('\n ', end='')
  print(' 0x%04x,' % (v1 * 256 + v0), end='')
print('\n};')
