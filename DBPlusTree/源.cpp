#define _CRT_SECURE_NO_WARNINGS

#include <iostream>
#include <random>
#include <ctime>

#include  "BPlusTree.h";

constexpr size_t KEY_NUMS = 1000;
constexpr size_t PER_PACKET_DELTAS = 100;
constexpr size_t INSET_COUNTER = 400000;
constexpr size_t QUERY_COUNTER = 4000000;

int main() {

	BPlusTree bpt;

	bpt.init("testdb");

	int packet_len = sizeof(DeltaPacket) + PER_PACKET_DELTAS * sizeof(DeltaItem);
	DeltaPacket* packet = (DeltaPacket*) new char[packet_len];

	auto bt = clock();
	
	for (size_t i = 0; i < INSET_COUNTER; ++i) {
		packet->version = i;
		for (size_t j = 0; j < PER_PACKET_DELTAS; ++j) {
			packet->deltas[j].key = rand() % KEY_NUMS;
			for (size_t k = 0; k < DATA_FIELD_NUM; ++k) {
				packet->deltas[j].delta[k] = rand() % 100;
			}
		}

		uint64_t Offset = bpt.datDwrite((char*)packet, packet_len);
		Offset += sizeof(DeltaPacket);

		for (size_t j = 0; j < PER_PACKET_DELTAS; ++j) {
			
			uint64_t L2RootOffset = bpt.find(packet->deltas[j].key);
			if (L2RootOffset == FNUL) {
				Pointer<Node> L2Root= bpt.L2init(packet->version, Offset, packet->deltas[j]);
				bpt.push(packet->deltas[j].key, L2Root.offset());
			}
			else {

				FPTR newL2RootOffset = bpt.push(L2RootOffset, packet->version, Offset);
				if (newL2RootOffset != L2RootOffset) {
					bpt.push(packet->deltas[j].key, newL2RootOffset);
				}
			}
			Offset += sizeof(DeltaItem);
		}

		if (i % 5000 == 0) {
			auto spend_time = 1.0 * (clock() - bt) / CLOCKS_PER_SEC;
			std::cout << "		write " << int(1.0 * i * PER_PACKET_DELTAS / spend_time) << " delta/s \n";
		}

	}


	return 0;
}

