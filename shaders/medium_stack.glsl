// Bit-encoded LIFO stack for medium IDs, stored in a single uint.
//
// Memory layout (BITS=4, MAX_DEPTH=7):
//
//   bit 31  30-28   27          0
//   |   |   |       |           |
//   [-] [top] [slot 6] ... [slot 0]
//              ^^^^^^^     ^^^^^^^
//              top-1 item  bottom (first pushed)
//
// Bits  0-27 : stack contents (7 slots x 4 bits = 28 bits)
// Bits 28-30 : top            (3 bits, range 0-7, covers MAX_DEPTH=7)
// Bit     31 : unused
//
// 'top' counts how many items are live.

const uint BITS      = 4u;
const uint MASK      = (1u << BITS) - 1u;
const int  MAX_DEPTH = 7;

const uint TOP_SHIFT = 28u;
const uint TOP_MASK  = 7u;

void stack_push(inout uint stack, inout int top, uint id) {
    stack |= (id & MASK) << (uint(top) * BITS);
    top++;
}

uint stack_peek(uint stack, int top) {
    return (stack >> (uint(top - 1) * BITS)) & MASK;
}

uint stack_pop(inout uint stack, inout int top) {
    top--;
    uint id = (stack >> (uint(top) * BITS)) & MASK;
    stack &= ~(MASK << (uint(top) * BITS));
    return id;
}

bool stack_empty(int top) {
    return top == 0;
}

uint stack_pack(uint stack, int top) {
    return (stack & 0x0FFFFFFFu) | (uint(top) << TOP_SHIFT);
}

void stack_unpack(uint encoded, out uint stack, out int top) {
    stack = encoded & 0x0FFFFFFFu;
    top   = int((encoded >> TOP_SHIFT) & TOP_MASK);
}
