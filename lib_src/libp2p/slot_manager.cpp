#include "slot_manager.hpp"

slot_manager::slot_manager(
	const int connection_ID_in,
	boost::function<void (boost::shared_ptr<message::base>)> send_in,
	boost::function<void (boost::shared_ptr<message::base>)> expect_in,
	boost::function<void (boost::shared_ptr<message::base>)> expect_anytime_in
):
	connection_ID(connection_ID_in),
	send(send_in),
	expect(expect_in),
	expect_anytime(expect_anytime_in),
	pipeline_cur(0),
	pipeline_max(protocol::max_block_pipeline / 2),
	open_slots(0),
	latest_slot(0)
{
	//register possible incoming messages
	expect_anytime(boost::shared_ptr<message::base>(new message::request_slot(
		boost::bind(&slot_manager::recv_request_slot, this, _1))));
/*
DEBUG, must be expected_anytime after slot message sent
	expect_anytime(boost::shared_ptr<message::base>(new message::request_hash_tree_block(
		boost::bind(&slot_manager::recv_request_hash_tree_block, this, _1)
	));
*/
}

void slot_manager::make_block_requests(boost::shared_ptr<slot> S)
{
	/*
	This function is always called after a block is received (except for the
	first call to send initial requests). The optimal pipeline size is 1 so that
	we have as little in the pipeline as we need to maintain max throughput. We
	adjust the max pipeline size to try to achieve the optimal pipeline size.

	Adaptive Pipeline Sizing Algorithm

	C = current requests unfulfilled (requests in pipeline)
	M = maximum pipeline size (this is what we adjust)
	A = absolute maximum pipeline size (M always < A)

	case: C == 0 && M < A
		//pipeline is too small, increase by one
		++M
	case: C == 1
		//pipeline size optimal, do nothing
	case: C > 1 && M > 1
		//pipeline is too large, decrease by one
		--M;
	*/
	if(pipeline_cur == 0 && pipeline_max < protocol::max_block_pipeline){
		++pipeline_max;
	}else if(pipeline_cur > 1 && pipeline_max > 1){
		--pipeline_max;
	}

	if(pipeline_cur >= pipeline_max){
		return;
	}
	if(Outgoing_Slot.empty()){
		return;
	}
	std::map<unsigned char, boost::shared_ptr<slot> >::iterator
		iter_cur = Outgoing_Slot.upper_bound(latest_slot);
	if(iter_cur == Outgoing_Slot.end()){
		iter_cur = Outgoing_Slot.begin();
	}

	/*
	These are used to break out of the loop if we loop through all slots and none
	need to make a request.
	*/
	unsigned char start_slot = iter_cur->first;
	bool serviced_one = false;

	while(true){
		if(pipeline_cur >= pipeline_max){
			break;
		}
		if(iter_cur->second->get_transfer()){
			boost::uint64_t block_num;   //block number to request
			boost::uint64_t block_count; //total number of blocks
			unsigned block_size;
			if(iter_cur->second->get_transfer()->request_hash_tree_block(connection_ID,
				block_num, block_size, block_count))
			{
				++pipeline_cur;
				serviced_one = true;
				boost::shared_ptr<message::base> M_request(new message::request_hash_tree_block(
					iter_cur->first, block_num, block_count));
				send(M_request);
				boost::shared_ptr<message::composite> M_response(new message::composite());
				M_response->add(boost::shared_ptr<message::block>(new message::block(boost::bind(
					&slot_manager::recv_hash_tree_block, this, _1, block_num), block_size)));
				M_response->add(boost::shared_ptr<message::error>(new message::error(
					boost::bind(&slot_manager::recv_request_block_failed, this, _1))));
				expect(M_response);
			}else if(iter_cur->second->get_transfer()->request_file_block(connection_ID,
				block_num, block_size, block_count))
			{
				++pipeline_cur;
				serviced_one = true;
				boost::shared_ptr<message::base> M_request(new message::request_file_block(
					iter_cur->first, block_num, block_count));
				send(M_request);
				boost::shared_ptr<message::composite> M_response(new message::composite());
				M_response->add(boost::shared_ptr<message::block>(new message::block(boost::bind(
					&slot_manager::recv_file_block, this, _1, block_num), block_size)));
				M_response->add(boost::shared_ptr<message::error>(new message::error(
					boost::bind(&slot_manager::recv_request_block_failed, this, _1))));
				expect(M_response);
			}
		}
		++iter_cur;
		if(iter_cur == Outgoing_Slot.end()){
			iter_cur = Outgoing_Slot.begin();
		}
		if(iter_cur->first == start_slot && !serviced_one){
			//looped through all sockets and none needed to be serviced
			break;
		}else if(iter_cur->first == start_slot && serviced_one){
			//reset flag for next loop through slots
			serviced_one = false;
		}
	}
}

void slot_manager::make_slot_requests()
{
	while(!Pending.empty() && open_slots < 256){
		share::slot_iterator slot_iter = share::singleton().find_slot(Pending.front());
		Pending.pop();
		if(slot_iter == share::singleton().end_slot()){
			continue;
		}
		++open_slots;
		boost::shared_ptr<message::base> M_request(new message::request_slot(
			slot_iter->hash()));
		send(M_request);
		boost::shared_ptr<message::composite> M_response(new message::composite());
		M_response->add(boost::shared_ptr<message::base>(new message::slot(
			boost::bind(&slot_manager::recv_slot, this, _1, slot_iter->hash()),
			slot_iter->hash())));
		M_response->add(boost::shared_ptr<message::base>(new message::error(
			boost::bind(&slot_manager::recv_request_slot_failed, this, _1))));
		expect(M_response);
	}
}

bool slot_manager::recv_file_block(boost::shared_ptr<message::base> M,
	const boost::uint64_t block_num)
{
LOGGER << block_num;
	return true;
}

bool slot_manager::recv_hash_tree_block(boost::shared_ptr<message::base> M,
	const boost::uint64_t block_num)
{
LOGGER << block_num;
	return true;
}

bool slot_manager::recv_request_block_failed(boost::shared_ptr<message::base> M)
{
LOGGER;
	return true;
}

bool slot_manager::recv_request_slot(boost::shared_ptr<message::base> M)
{
	//requested hash
	std::string hash = convert::bin_to_hex(
		reinterpret_cast<const char *>(M->buf.data()+1), SHA1::bin_size);
	LOGGER << hash;

	//locate requested slot
	share::slot_iterator slot_iter = share::singleton().find_slot(hash);
	if(slot_iter == share::singleton().end_slot()){
		LOGGER << "failed " << hash;
		boost::shared_ptr<message::base> M(new message::error());
		send(M);
		return true;
	}

	if(!slot_iter->get_transfer()){
		//we do not yet know file size
		LOGGER << "failed " << hash;
		boost::shared_ptr<message::base> M(new message::error());
		send(M);
		return true;
	}

	//find available slot number
	unsigned char slot_num = 0;
	for(std::map<unsigned char, boost::shared_ptr<slot> >::iterator
		iter_cur = Incoming_Slot.begin(), iter_end = Incoming_Slot.end();
		iter_cur != iter_end; ++iter_cur)
	{
		if(iter_cur->first != slot_num){
			break;
		}
		++slot_num;
	}

	//status byte (third byte of slot message)
	unsigned char status;
	if(!slot_iter->get_transfer()->status(status)){
		LOGGER << "failed " << hash;
		boost::shared_ptr<message::base> M(new message::error());
		send(M);
		return true;
	}

	//file size
	boost::uint64_t file_size = slot_iter->file_size();
	if(file_size == 0){
		LOGGER << "failed " << hash;
		boost::shared_ptr<message::base> M(new message::error());
		send(M);
		return true;
	}

	//root hash
	std::string root_hash;
	if(!slot_iter->get_transfer()->root_hash(root_hash)){
		LOGGER << "failed " << hash;
		boost::shared_ptr<message::base> M(new message::error());
		send(M);
		return true;
	}

	//we have all information to send slot message
	boost::shared_ptr<message::base> M_send(new message::slot(slot_num, status,
		file_size, root_hash));
	send(M_send);

	//add slot
	std::pair<std::map<unsigned char, boost::shared_ptr<slot> >::iterator, bool>
		ret = Incoming_Slot.insert(std::make_pair(slot_num, slot_iter.get()));
	assert(ret.second);
	return true;
}

bool slot_manager::recv_request_slot_failed(boost::shared_ptr<message::base> M)
{
LOGGER << "stub: handle request slot failure by removing source";
	--open_slots;
	return true;
}

bool slot_manager::recv_slot(boost::shared_ptr<message::base> M,
	const std::string hash)
{
	share::slot_iterator slot_iter = share::singleton().find_slot(hash);
	if(slot_iter == share::singleton().end_slot()){
		LOGGER << "failed " << hash;
		boost::shared_ptr<message::base> M(new message::error());
		send(M);
		return true;
	}
	std::pair<std::map<unsigned char, boost::shared_ptr<slot> >::iterator, bool>
		ret = Outgoing_Slot.insert(std::make_pair(M->buf[1], slot_iter.get()));
	if(ret.second == false){
		//host sent duplicate slot
		LOGGER << "violated protocol";
		return false;
	}
	LOGGER << hash;
	//file size and root hash might not be known, set them
	boost::uint64_t file_size = convert::decode<boost::uint64_t>(
		std::string(reinterpret_cast<char *>(M->buf.data()+3), 8));
	std::string root_hash = convert::bin_to_hex(
		reinterpret_cast<char *>(M->buf.data()+11), SHA1::bin_size);
	LOGGER << "file_size: " << file_size;
	LOGGER << "root_hash: " << root_hash;
	if(!slot_iter->set_unknown(file_size, root_hash)){
LOGGER << "stub: need to send close slot here";
		return true;
	}
	if(M->buf[2] == 0){
		slot_iter->get_transfer()->register_outgoing_0(connection_ID);
	}else if(M->buf[2] == 1){
LOGGER << "stub: add support for incomplete"; exit(1);
	}else if(M->buf[2] == 2){
LOGGER << "stub: add support for incomplete"; exit(1);
	}else if(M->buf[2] == 3){
LOGGER << "stub: add support for incomplete"; exit(1);
	}
	make_block_requests(slot_iter.get());
	return true;
}

void slot_manager::resume(const std::string & peer_ID)
{
	std::set<std::string> hash = database::table::join::resume_hash(peer_ID);
	for(std::set<std::string>::iterator iter_cur = hash.begin(), iter_end = hash.end();
		iter_cur != iter_end; ++iter_cur)
	{
		Pending.push(*iter_cur);
	}
	make_slot_requests();
}
