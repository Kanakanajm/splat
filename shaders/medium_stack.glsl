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
// Bits  0-27 : stack contents (7 slots × 4 bits = 28 bits)
// Bits 28-30 : top            (3 bits, range 0-7, covers MAX_DEPTH=7)
// Bit     31 : unused
//
// 'top' counts how many items are live.
// Preconditions:
//   stack_peek / stack_pop : top > 0
//   stack_push             : top < MAX_DEPTH

const uint BITS      = 4u;                // bits per slot; IDs 0–15
const uint MASK      = (1u << BITS) - 1u; // 0xF — isolates one slot
const int  MAX_DEPTH = 7;                 // 4 * 7 = 28 bits, leaving 4 bits for top

const uint TOP_SHIFT = 28u;              // first bit of the top field
const uint TOP_MASK  = 7u;              // 3 bits, covers 0..7

// Push id onto the stack.
void stack_push(inout uint stack, inout int top, uint id) {
    // Write id into slot [top]: bits [top*BITS .. (top+1)*BITS)
    stack |= (id & MASK) << (uint(top) * BITS);
    top++;
}

// Return the top item without removing it.
uint stack_peek(uint stack, int top) {
    // Slot [top-1] lives in bits [(top-1)*BITS .. top*BITS)
    return (stack >> (uint(top - 1) * BITS)) & MASK;
}

// Remove and return the top item.
uint stack_pop(inout uint stack, inout int top) {
    top--;
    // Read slot [top]: bits [top*BITS .. (top+1)*BITS)
    uint id = (stack >> (uint(top) * BITS)) & MASK;
    // Zero those bits so the slot is clean for future pushes
    stack &= ~(MASK << (uint(top) * BITS));
    return id;
}

// True when no items have been pushed.
bool stack_empty(int top) {
    return top == 0; // top is the item count directly
}

// Encode (stack, top) into one uint for storage in a GL_R32UI texel.
uint stack_pack(uint stack, int top) {
    // Top goes into bits 28-30; stack contents in bits 0-27
    return (stack & 0x0FFFFFFFu) | (uint(top) << TOP_SHIFT);
}

// Decode a encoded texel back into (stack, top).
void stack_unpack(uint encoded, out uint stack, out int top) {
    // Stack is the low 28 bits; top is bits 28-30
    stack = encoded & 0x0FFFFFFFu;
    top   = int((encoded >> TOP_SHIFT) & TOP_MASK);
}

// --- Test ---------------------------------------------------------------
// Scenario: medium 3 is outermost, 2 is inside 3, 1 is inside 2.
// peek must return the innermost ID, not the highest-numbered one.
//
void test_stack() {
    uint stack = 0u;
    int  top   = 0;

    bool e0 = stack_empty(top);           // expected: true

    stack_push(stack, top, 3u);           // stack: [3]       top=1
    stack_push(stack, top, 2u);           // stack: [3, 2]    top=2
    stack_push(stack, top, 1u);           // stack: [3, 2, 1] top=3

    uint p0 = stack_peek(stack, top);     // expected: 1u  (innermost, not findMSB=3)

    uint v0 = stack_pop(stack, top);      // expected: 1u    stack: [3, 2]  top=2

    uint p1 = stack_peek(stack, top);     // expected: 2u

    uint v1 = stack_pop(stack, top);      // expected: 2u    stack: [3]     top=1
    uint v2 = stack_pop(stack, top);      // expected: 3u    stack: []      top=0

    bool e1 = stack_empty(top);           // expected: true

    // Round-trip through pack/unpack
    stack_push(stack, top, 5u);
    stack_push(stack, top, 9u);
    uint encoded = stack_pack(stack, top);
    uint s2; int t2;
    stack_unpack(encoded, s2, t2);
    uint p2 = stack_peek(s2, t2);         // expected: 9u
}
