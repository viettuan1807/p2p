#include "k_func.hpp"

extern bit_field k_func::ID_to_bit_field(const std::string & ID)
{
	assert(ID.size() == SHA1::hex_size);
	std::string bin = convert::hex_to_bin(ID);
	bit_field BF(reinterpret_cast<const unsigned char *>(bin.data()), bin.size(),
		protocol_udp::bucket_count);
	return BF;
}

extern unsigned k_func::ID_to_bucket_num(const std::string & ID_0,
	const std::string & ID_1)
{
	assert(ID_0.size() == SHA1::hex_size);
	assert(ID_1.size() == SHA1::hex_size);
	bit_field BF_0 = ID_to_bit_field(ID_0);
	bit_field BF_1 = ID_to_bit_field(ID_1);
	for(int x=protocol_udp::bucket_count - 1; x>=0; --x){
		if(BF_0[x] != BF_1[x]){
			return x;
		}
	}
	return 0;
}
