#include <iostream>
#include <bitset>


#define KEY_SIZE_BITS		16

#define BITMASK(name, type, field, offset, size)		\
static inline uint64_t name(const type *k)				\
{ return (k->field >> offset) & ~(~0ULL << size); }		\


struct bkey {
	uint64_t	high;
	uint64_t	low;
	uint64_t	ptr[];
};


#define KEY_FIELD(name, field, offset, size)				\
	BITMASK(name, struct bkey, field, offset, size)


KEY_FIELD(KEY_PTRS,	high, 60, 3)
KEY_FIELD(__PAD0,	high, 58, 2)
KEY_FIELD(KEY_CSUM,	high, 56, 2)
KEY_FIELD(__PAD1,	high, 55, 1)
KEY_FIELD(KEY_DIRTY,	high, 36, 1)

KEY_FIELD(KEY_SIZE,	high, 20, KEY_SIZE_BITS)
KEY_FIELD(KEY_INODE,	high, 0,  20)

uint64_t keys[3];

int main()
{
	struct bkey *b = (struct bkey *)&keys;

	b->high = 11686030448663912173ULL;
	b->low = 11299865772490926784ULL;
	b->ptr[0] = 15292707573528821862ULL;

	std::cout << "HIGH LOW PTR" << std::endl;
	std::cout << "0b" << std::bitset<64>(b->high) << std::endl;
	std::cout << "0b" << std::bitset<64>(b->low) << std::endl;
	std::cout << "0b" << std::bitset<64>(b->ptr[0]) << std::endl;

	std::cout << "PTRS" << std::endl;
	std::cout << "0b" << std::bitset<64>(b->high >> 60) << std::endl;
	std::cout << "0b" << std::bitset<64>(~(~0ULL << 3)) << std::endl;
	std::cout << "0b" << std::bitset<64>(KEY_PTRS(b)) << std::endl;

	std::cout << "DIRTY" << std::endl;
	std::cout << "0b" << std::bitset<64>(b->high >> 36) << std::endl;
	std::cout << "0b" << std::bitset<64>(~(~0ULL << 1)) << std::endl;
	std::cout << "0b" << std::bitset<64>(KEY_DIRTY(b)) << std::endl;

	std::cout << "SIZE" << std::endl;
	std::cout << "0b" << std::bitset<64>(b->high >> 20) << std::endl;
	std::cout << "0b" << std::bitset<64>(~(~0ULL << 16)) << std::endl;
	std::cout << "0b" << std::bitset<64>(KEY_SIZE(b)) << std::endl;

	std::cout << "INODE" << std::endl;
	std::cout << "0b" << std::bitset<64>(b->high >> 0) << std::endl;
	std::cout << "0b" << std::bitset<64>(~(~0ULL << 20)) << std::endl;
	std::cout << "0b" << std::bitset<64>(KEY_INODE(b)) << std::endl;


	return 0;
}
