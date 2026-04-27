#include <systemc>
#include <tlm>
#include <tlm_utils/simple_initiator_socket.h>
#include <tlm_utils/simple_target_socket.h>
#include <deque>
#include <list>

using namespace sc_core;
using namespace tlm;
using namespace tlm_utils;

class A : public sc_module {
	public:
		SC_HAS_PROCESS(A);
		A(sc_module_name name) : sc_module(name), encode(0), decode(0), cnt(0), queue_id(0) {
			SC_METHOD(process);
			sensitive << m_clk.pos();
			dont_initialize();
		}

		~A() {
		}

		void process();

	public:
		sc_in_clk m_clk;
		int encode;
		int decode;
		int cnt;
		int queue_id;
		simple_initiator_socket<A> tx;
		std::deque<int> decode_que;
};

void A::process() {
	cnt++;
	queue_id++;
	if (cnt == 2) {
		encode = 1;
	}

	if (encode) {
		tlm_generic_payload pl;
		pl.set_command(TLM_WRITE_COMMAND);
		pl.set_data_length(1);
		//unsigned char data[] = {0x12, 0x34, 0x56, 0x78};
		pl.set_data_ptr((unsigned char*)&queue_id);
		pl.set_address(0x1000);
		tlm_phase phase = BEGIN_REQ;
		sc_time delay = SC_ZERO_TIME;
		tx->nb_transport_fw(pl, phase, delay);
		decode_que.push_back(queue_id);
	}

	if (!decode_que.empty()) {
		tlm_generic_payload pl;
		pl.set_command(TLM_WRITE_COMMAND);
		pl.set_data_length(1);
		int queue_id = decode_que.front();
		decode_que.pop_front();
		//unsigned char data[] = {0x12, 0x34, 0x56, 0x78};
		pl.set_data_ptr((unsigned char*)&queue_id);
		pl.set_address(0x2000);
		tlm_phase phase = BEGIN_REQ;
		sc_time delay = SC_ZERO_TIME;
		tx->nb_transport_fw(pl, phase, delay);
	}
}

struct bank {
	int bank_id;
	int slice_num;
	int read;
};

struct slice {
	int valid;
	int bank_id;
	int slice_id;
	int next_bank_id;
	int next_slice_id;
	int queue_id;
};

struct queue_info {
	int bank_id;
	int slice_id;
	int counter;
};

class B : public sc_module {
	public:
		SC_HAS_PROCESS(B);
		B(sc_module_name name) : sc_module(name), bank_num(4), slice_num(128), decode_link_num(5), delay(4), write_cnt(1), write_slice_num(16) {
			rx.register_nb_transport_fw(this, &B::rcv_from);
			SC_METHOD(process);
			sensitive << m_clk.pos();
			dont_initialize();
			encode_vec.resize(bank_num);
			bank_deep_vec.resize(bank_num);
			read_bank_id_vec.resize(bank_num, 0);
			for(int i = 0; i < bank_num; i++) {
				encode_vec[i].resize(slice_num);
				bank_deep_vec[i] = slice_num;
				for (int j = 0; j < slice_num; j++) {
					encode_vec[i][j].valid = 0;
					encode_vec[i][j].bank_id = -1;
					encode_vec[i][j].slice_id = -1;
					encode_vec[i][j].next_bank_id = -1;
					encode_vec[i][j].next_slice_id = -1;
					encode_vec[i][j].queue_id = -1;
				}
			}
		}

		~B() {
		}

		void process();
		tlm_sync_enum rcv_from(tlm_generic_payload& pl, tlm_phase& phase, sc_time& delay);

		void encode_list();
		void decode_list();

	public:
		int decode_link_num;
		int read_bank_id;
		int delay;
		int bank_num;
		int slice_num;
		sc_in_clk m_clk;
		int encode;
		int decode;
		simple_target_socket<B> rx;
		std::deque<int> encode_que;
		std::deque<int> decode_que;
		std::vector<std::vector<slice>> encode_vec;
		std::vector<int> bank_deep_vec;
		std::map<int, queue_info>	queue_map;
		std::list<int> decode_que_head;
		std::vector<int> read_bank_id_vec;

		int write_cnt;
		int write_slice_num;

		int last_bank_id = -1;
		int last_slice_id = -1;
};

void B::encode_list() {
}

void B::decode_list() {
}

void B::process() {
	if (!decode_que.empty() && (decode_que.size() > decode_link_num)) {
		while (decode_que_head.size() < decode_link_num) {
			int queue_id = decode_que.front();
			decode_que_head.push_back(queue_id);
			decode_que.pop_front();
		}
	}

	for (auto it = decode_que_head.begin(); it != decode_que_head.end();) {
		int queue_id = *it;
		if (queue_map.find(queue_id) == queue_map.end()) {
			++it;
			continue;
		}

		queue_info& bank_slice = queue_map[queue_id];
		if (bank_slice.counter > 0) {
			bank_slice.counter--;
			std::cout << "time: " << sc_time_stamp() << " B queue:" << queue_id << " read bank: " << bank_slice.bank_id << " counter: " << bank_slice.counter << std::endl;
			if  (bank_slice.counter != 0) {
				++it;
				continue;
			}
		}

		int bank_id = bank_slice.bank_id;
		int slice_id = bank_slice.slice_id;
		read_bank_id = bank_id;

		if (read_bank_id_vec[read_bank_id] == 1) {
			std::cout << "time: " << sc_time_stamp() << " B queue:" << queue_id << " read conflict on bank: " << read_bank_id << std::endl;
			++it;
			bank_slice.counter = 1;
			continue;
		}
		read_bank_id_vec[read_bank_id] = 1;

		int next_bank_id = encode_vec[bank_id][slice_id].next_bank_id;
		int next_slice_id = encode_vec[bank_id][slice_id].next_slice_id;
		encode_vec[bank_id][slice_id].valid = 0;
		encode_vec[bank_id][slice_id].bank_id = -1;
		encode_vec[bank_id][slice_id].slice_id = -1;
		encode_vec[bank_id][slice_id].next_bank_id = -1;
		encode_vec[bank_id][slice_id].next_slice_id = -1;
		encode_vec[bank_id][slice_id].queue_id = -1;
		bank_deep_vec[bank_id]++;
		std::cout << "time: " << sc_time_stamp() << " B queue:" << queue_id << " read release bank: " << bank_id << ", slice: " << slice_id << std::endl;

		bank_id = next_bank_id;
		slice_id = next_slice_id;
		
		if (bank_id == -1 || slice_id == -1) {
			std::cout << "time: " << sc_time_stamp() << " B queue:" << queue_id << " read decode erase: queue_id " << queue_id << std::endl;
			queue_map.erase(queue_id);
			it = decode_que_head.erase(it);
		} else {
			queue_map[queue_id] = {bank_id, slice_id, delay};
			++it;
		}
	}

	if (!encode_que.empty()) {
		int queue_id = encode_que.front();
		if (write_cnt > 0) {
			write_cnt--;
			std::cout << "time: " << sc_time_stamp() << " B queue:" << queue_id << " write wait counter: " << write_cnt << std::endl;
			if (write_cnt == 0) {
				write_cnt = 1;

				std::cout << "time: " << sc_time_stamp() << " B queue:" << queue_id << " process encode data: " << queue_id << std::endl;

				// 挑选bank_id,优先级为bank_deep_vec较大的bank_id，且不在读的bank
				int bank_id = 0;
				int max_deep = 0;
				for (int i = bank_id; i < bank_num; i++) {
					if (read_bank_id_vec[i] == 1) {
						continue;
					}

					if (bank_deep_vec[i] > max_deep) {
						max_deep = bank_deep_vec[i];
						bank_id = i;
					}
				}

				for (int j = 0; j < slice_num; j++) {
					if (encode_vec[bank_id][j].valid == 0) {
						encode_vec[bank_id][j].valid = 1;
						encode_vec[bank_id][j].bank_id = bank_id;
						encode_vec[bank_id][j].slice_id = j;
						encode_vec[bank_id][j].next_bank_id = -1;
						encode_vec[bank_id][j].next_slice_id = -1;
						encode_vec[bank_id][j].queue_id = queue_id;

						if (last_bank_id == -1 && last_slice_id == -1) {
							queue_map[queue_id] = {bank_id, j, delay};
						}

						if ((last_bank_id != -1) && (last_slice_id != -1)) {
							encode_vec[last_bank_id][last_slice_id].next_bank_id = bank_id;
							encode_vec[last_bank_id][last_slice_id].next_slice_id = j;
						}
						last_bank_id = bank_id;
						last_slice_id = j;
						
						bank_deep_vec[bank_id]--;
						max_deep = bank_deep_vec[bank_id];
						std::cout << "time: " << sc_time_stamp() << " B queue:" << queue_id << " write allocate bank: " << bank_id << ", slice: " << j << std::endl;
						break;
					}
				}

				write_slice_num--;
				if (write_slice_num == 0) {
					write_slice_num = 16;
					encode_que.pop_front();
					decode_que.push_back(queue_id);
					last_bank_id = -1;
					last_slice_id = -1;
				}
			}
		}
		
	}

	for (int i = 0; i < bank_num; i++) {
		read_bank_id_vec[i] = 0;
	}
}

tlm_sync_enum B::rcv_from(tlm_generic_payload& pl, tlm_phase& phase, sc_time& delay) {
	uint64_t addr = pl.get_address();
	unsigned char* data_ptr = pl.get_data_ptr();
	int queue_id = *reinterpret_cast<int*>(data_ptr);
	if (addr == 0x1000) {
		std::cout << "time: " << sc_time_stamp() << " B encode queue: " << queue_id << std::endl;
		encode_que.push_back(queue_id);
	} else if (addr == 0x2000) {
		std::cout << "time: " << sc_time_stamp() << " B decode queue: " << queue_id << std::endl;
	}
	return TLM_COMPLETED;
}

class TOP : public sc_module {
	public:
		SC_HAS_PROCESS(TOP);
		TOP(sc_module_name name) : sc_module(name), a("a"), b("b"), m_clk("clk", 1, SC_NS) {
			a.m_clk(m_clk);
			b.m_clk(m_clk);
			a.tx.bind(b.rx);
		}

	public:
		sc_clock m_clk;
		A a;
		B b;
};

int sc_main(int argc, char* argv[])
{
	TOP top("top");
	sc_start(300, SC_NS);
	//sc_start();
	return 0;
}
