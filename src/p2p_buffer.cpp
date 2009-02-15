#include "p2p_buffer.hpp"

//BEGIN STATIC
boost::mutex & p2p_buffer::Mutex()
{
	static boost::mutex * M = new boost::mutex();
	return *M;
}

std::map<int, boost::shared_ptr<p2p_buffer> > & p2p_buffer::P2P_Buffer()
{
	static std::map<int, boost::shared_ptr<p2p_buffer> > * B =
		new std::map<int, boost::shared_ptr<p2p_buffer> >();
	return *B;
}

int & p2p_buffer::send_pending()
{
	static int * SP = new int(0);
	return *SP;
}

std::set<boost::shared_ptr<download> > & p2p_buffer::Unique_Download()
{
	static std::set<boost::shared_ptr<download> > * UD =
		new std::set<boost::shared_ptr<download> >();
	return *UD;
}

void p2p_buffer::add_connection(const int & socket_FD, const std::string & IP)
{
	boost::mutex::scoped_lock lock(Mutex());
	std::map<int, boost::shared_ptr<p2p_buffer> >::iterator CB_iter;
	CB_iter = P2P_Buffer().find(socket_FD);
	if(CB_iter == P2P_Buffer().end()){
		std::pair<std::map<int, boost::shared_ptr<p2p_buffer> >::iterator, bool> ret;
		ret = P2P_Buffer().insert(std::make_pair(socket_FD, new p2p_buffer(socket_FD, IP)));
		assert(ret.second);
		CB_iter = ret.first;
	}
}

void p2p_buffer::add_download_connection(download_connection & DC)
{
	boost::mutex::scoped_lock lock(Mutex());
	std::map<int, boost::shared_ptr<p2p_buffer> >::iterator iter_cur, iter_end;
	iter_cur = P2P_Buffer().begin();
	iter_end = P2P_Buffer().end();
	while(iter_cur != iter_end){
		if(DC.IP == iter_cur->second->IP){
			DC.socket = iter_cur->second->socket;             //set socket of the discovered p2p_buffer
			DC.Download->register_connection(DC);             //register connection with download
			iter_cur->second->register_download(DC.Download); //register download with p2p_buffer
			break;
		}
		++iter_cur;
	}
}

void p2p_buffer::add_download(boost::shared_ptr<download> & Download)
{
	boost::mutex::scoped_lock lock(Mutex());
	Unique_Download().insert(Download);
}

void p2p_buffer::current_downloads(std::vector<download_status> & info, std::string hash)
{
	boost::mutex::scoped_lock lock(Mutex());
	info.clear();
	if(hash.empty()){
		//info for all downloads
		std::set<boost::shared_ptr<download> >::iterator iter_cur, iter_end;
		iter_cur = Unique_Download().begin();
		iter_end = Unique_Download().end();
		while(iter_cur != iter_end){
			if((*iter_cur)->is_visible()){
				download_status Download_Status(
					(*iter_cur)->hash(),
					(*iter_cur)->name(),
					(*iter_cur)->size(),
					(*iter_cur)->speed(),
					(*iter_cur)->percent_complete()
				);
				(*iter_cur)->servers(Download_Status.IP, Download_Status.speed);
				info.push_back(Download_Status);
			}
			++iter_cur;
		}
	}else{
		//info for one download
		std::set<boost::shared_ptr<download> >::iterator iter_cur, iter_end;
		iter_cur = Unique_Download().begin();
		iter_end = Unique_Download().end();
		while(iter_cur != iter_end){
			if((*iter_cur)->hash() == hash){
				download_status Download_Status(
					(*iter_cur)->hash(),
					(*iter_cur)->name(),
					(*iter_cur)->size(),
					(*iter_cur)->speed(),
					(*iter_cur)->percent_complete()
				);
				(*iter_cur)->servers(Download_Status.IP, Download_Status.speed);
				info.push_back(Download_Status);
				break;
			}
			++iter_cur;
		}
	}
}

void p2p_buffer::current_uploads(std::vector<upload_info> & info)
{
	boost::mutex::scoped_lock lock(Mutex());
	std::map<int, boost::shared_ptr<p2p_buffer> >::iterator iter_cur, iter_end;
	iter_cur = P2P_Buffer().begin();
	iter_end = P2P_Buffer().end();
	while(iter_cur != iter_end){
		iter_cur->second->uploads(info);
		++iter_cur;
	}
}

void p2p_buffer::erase(const int & socket_FD)
{
	boost::mutex::scoped_lock lock(Mutex());
	std::map<int, boost::shared_ptr<p2p_buffer> >::iterator iter = P2P_Buffer().find(socket_FD);
	if(iter != P2P_Buffer().end()){
		if(!iter->second->send_buff.empty()){
			//send_buff contains data, decrement send_pending()
			--send_pending();
		}
		P2P_Buffer().erase(iter);
	}
}

bool p2p_buffer::is_connected(const std::string & IP)
{
	std::map<int, boost::shared_ptr<p2p_buffer> >::iterator iter_cur, iter_end;
	iter_cur = P2P_Buffer().begin();
	iter_end = P2P_Buffer().end();
	while(iter_cur != iter_end){
		if(iter_cur->second->IP == IP){
			return true;
		}
		++iter_cur;
	}
	return false;
}

bool p2p_buffer::is_downloading(boost::shared_ptr<download> & Download)
{
	boost::mutex::scoped_lock lock(Mutex());
	std::set<boost::shared_ptr<download> >::iterator iter = Unique_Download().find(Download);
	return iter != Unique_Download().end();
}

bool p2p_buffer::is_downloading(const std::string & hash)
{
	boost::mutex::scoped_lock lock(Mutex());
	std::set<boost::shared_ptr<download> >::iterator iter_cur, iter_end;
	iter_cur = Unique_Download().begin();
	iter_end = Unique_Download().end();
	while(iter_cur != iter_end){
		if((*iter_cur)->hash() == hash){
			return true;
		}
		++iter_cur;
	}
	return false;
}

void p2p_buffer::get_complete_downloads(std::vector<boost::shared_ptr<download> > & complete)
{
	boost::mutex::scoped_lock lock(Mutex());
	std::set<boost::shared_ptr<download> >::iterator UD_iter_cur, UD_iter_end;
	UD_iter_cur = Unique_Download().begin();
	UD_iter_end = Unique_Download().end();
	while(UD_iter_cur != UD_iter_end){
		if((*UD_iter_cur)->complete()){
			complete.push_back(*UD_iter_cur);

			//remove complete download from all p2p_buffers
			std::map<int, boost::shared_ptr<p2p_buffer> >::iterator CB_iter_cur, CB_iter_end;
			CB_iter_cur = P2P_Buffer().begin();
			CB_iter_end = P2P_Buffer().end();
			while(CB_iter_cur != CB_iter_end){
				CB_iter_cur->second->terminate_download(*UD_iter_cur);
				++CB_iter_cur;
			}

			//remove from unique download
			Unique_Download().erase(UD_iter_cur++);
		}else{
			++UD_iter_cur;
		}
	}
}

int p2p_buffer::get_send_pending()
{
	boost::mutex::scoped_lock lock(Mutex());
	assert(send_pending() >= 0);
	return send_pending();
}

bool p2p_buffer::get_send_buff(const int & socket_FD, const int & max_bytes, std::string & destination)
{
	boost::mutex::scoped_lock lock(Mutex());
	destination.clear();
	std::map<int, boost::shared_ptr<p2p_buffer> >::iterator iter = P2P_Buffer().find(socket_FD);
	if(iter == P2P_Buffer().end()){
		return false;
	}else{
		int size;
		if(max_bytes > iter->second->send_buff.size()){
			size = iter->second->send_buff.size();
		}else{
			size = max_bytes;
		}
		destination.assign(iter->second->send_buff.data(), size);
		return !destination.empty();
	}
}

void p2p_buffer::get_timed_out(std::vector<int> & timed_out)
{
	boost::mutex::scoped_lock lock(Mutex());
	std::map<int, boost::shared_ptr<p2p_buffer> >::iterator iter_cur, iter_end;
	iter_cur = P2P_Buffer().begin();
	iter_end = P2P_Buffer().end();
	while(iter_cur != iter_end){
		if(time(NULL) - iter_cur->second->last_seen >= global::TIMEOUT){
			timed_out.push_back(iter_cur->first);
			P2P_Buffer().erase(iter_cur++);
		}else{
			++iter_cur;
		}
	}
}

bool p2p_buffer::post_send(const int & socket_FD, const int & n_bytes)
{
	boost::mutex::scoped_lock lock(Mutex());
	std::map<int, boost::shared_ptr<p2p_buffer> >::iterator iter = P2P_Buffer().find(socket_FD);
	if(iter != P2P_Buffer().end()){
		iter->second->send_buff.erase(0, n_bytes);
		if(n_bytes && iter->second->send_buff.empty()){
			//buffer went from non-empty to empty
			--send_pending();
		}
		if(iter->second->http_response_sent && iter->second->send_buff.empty()){
			return true;
		}
	}
	return false;
}

void p2p_buffer::post_recv(const int & socket_FD, char * buff, const int & n_bytes)
{
	boost::mutex::scoped_lock lock(Mutex());
	std::map<int, boost::shared_ptr<p2p_buffer> >::iterator iter = P2P_Buffer().find(socket_FD);
	if(iter != P2P_Buffer().end()){
		iter->second->recv_buff_process(buff, n_bytes);
	}
}

void p2p_buffer::process()
{
	boost::mutex::scoped_lock lock(Mutex());
	std::map<int, boost::shared_ptr<p2p_buffer> >::iterator iter_cur, iter_end;
	iter_cur = P2P_Buffer().begin();
	iter_end = P2P_Buffer().end();
	while(iter_cur != iter_end){
		iter_cur->second->prepare_download_requests();
		++iter_cur;
	}
}

void p2p_buffer::stop_download(const std::string & hash)
{
	boost::mutex::scoped_lock lock(Mutex());
	std::set<boost::shared_ptr<download> >::iterator iter_cur, iter_end;
	iter_cur = Unique_Download().begin();
	iter_end = Unique_Download().end();
	while(iter_cur != iter_end){
		if(hash == (*iter_cur)->hash()){
			(*iter_cur)->stop();
			return;
		}
		++iter_cur;
	}
}
//END STATIC

p2p_buffer::p2p_buffer()
{
	LOGGER << "improperly constructed p2p_buffer";
	exit(1);
}

p2p_buffer::p2p_buffer(
	const int & socket_in,
	const std::string & IP_in
):
	socket(socket_in),
	IP(IP_in),
	bytes_seen(0),
	last_seen(time(NULL)),
	download_slots_used(0),
	max_pipeline_size(1),
	exchange_key(true),
	http_response_sent(false)
{
	recv_buff.reserve(global::MAX_MESSAGE_SIZE * global::PIPELINE_SIZE);
	send_buff.reserve(global::MAX_MESSAGE_SIZE * global::PIPELINE_SIZE);

	//look at encryption.h for key exchange protocol
/*
	send_buff += Encryption.get_prime();
	Encryption.set_prime(send_buff);
	send_buff += Encryption.get_local_result();
	++send_pending();
*/
	for(int x=0; x<256; ++x){
		Upload_Slot[x] = NULL;
	}
}

p2p_buffer::~p2p_buffer()
{
	//unregister connection with all downloads
	std::list<boost::shared_ptr<download> >::iterator iter_cur, iter_end;
	iter_cur = Download.begin();
	iter_end = Download.end();
	while(iter_cur != iter_end){
		(*iter_cur)->unregister_connection(socket);
		++iter_cur;
	}

	//free all allocated upload slots
	for(int x=0; x<256; ++x){
		if(Upload_Slot[x] != NULL){
			delete Upload_Slot[x];
		}
	}
}

void p2p_buffer::close_slot(const std::string & request)
{
	if(Upload_Slot[(int)(unsigned char)request[1]] != NULL){
		delete Upload_Slot[(int)(unsigned char)request[1]];
		Upload_Slot[(int)(unsigned char)request[1]] = NULL;
	}else{
		LOGGER << IP << " attempted to close slot it didn't have open";
		DB_Blacklist.add(IP);
	}
}

bool p2p_buffer::find_empty_slot(const std::string & root_hash, int & slot_num)
{
	//check to make sure slot for file not already requested
	for(int x=0; x<256; ++x){
		if(Upload_Slot[x] != NULL && Upload_Slot[x]->get_hash() == root_hash){
			LOGGER << IP << " requested slot for " << root_hash << " twice";
			DB_Blacklist.add(IP);
			return false;
		}
	}

	//find empty slot
	for(int x=0; x<256; ++x){
		if(Upload_Slot[x] == NULL){
			slot_num = x;
			return true;
		}
	}

	LOGGER << IP << " requested more than 256 slots";
	DB_Blacklist.add(IP);
	return false;
}

void p2p_buffer::prepare_download_requests()
{
/*
	if(exchange_key){
		return;
	}
*/
	if(Download.empty()){
		return;
	}

	//see header documentation for max_pipeline_size
	if(Pipeline.size() == 0 && max_pipeline_size < global::PIPELINE_SIZE){
		++max_pipeline_size;
	}else if(Pipeline.size() > 1 && max_pipeline_size > 1){
		--max_pipeline_size;
	}

	bool buffer_change = true;
	int initial_empty = send_buff.empty();
	while(Pipeline.size() < max_pipeline_size){
		if(rotate_downloads()){
			/*
			When rotate_downloads returns true it means that a full rotation of the
			ring buffer has been done. If the buffer has not been modified after one
			full rotation it means that all the downloads have no requests to make.
			*/
			if(!buffer_change){
				break;
			}
			buffer_change = false;
		}

		if((*Download_iter)->complete()){
			//download complete, don't attempt to get a request
			continue;
		}

		std::string request;
		pending_response PR;
		PR.Download = *Download_iter;
		PR.Mode = (*Download_iter)->request(socket, request, PR.expected, download_slots_used);
		assert(download_slots_used >= 0 && download_slots_used <= 255);

		//Encryption.crypt_send(request);

		if(PR.Mode == download::NO_REQUEST){
			//no request to be made from the download
			continue;
		}else{
			send_buff += request;
			if(PR.Mode == download::BINARY_MODE){
				if(!PR.expected.empty()){
					//no response expected, don't add to pipeline
					Pipeline.push_back(PR);
				}
			}else if(PR.Mode == download::TEXT_MODE){
				//text mode always expects a response
				Pipeline.push_back(PR);
			}
			buffer_change = true;
		}
	}

	if(initial_empty && !send_buff.empty()){
		++send_pending();
	}
}

void p2p_buffer::recv_buff_process(char * buff, const int & n_bytes)
{
	last_seen = time(NULL);
	bool initial_empty = send_buff.empty();
	if(IP.find("127") == 0){
		//localhost obeys entirely different protocol
		if(http_response_sent){
			return;
		}

		recv_buff.append(buff, n_bytes);
		if(recv_buff.size() > 1024*1024){
			//very generous sanity check for local recv buff
			recv_buff.clear();
		}
		if(recv_buff.find("\n\r") != std::string::npos){
			//http request ends with \n\r
			HTTP.process(recv_buff, send_buff);
			http_response_sent = true;
		}
		if(initial_empty && !send_buff.empty()){
			++send_pending();
		}
		return;
	}

/*
	if(exchange_key){
		remote_result.append(buff, n_bytes);
		if(remote_result.size() == global::DH_KEY_SIZE){
			Encryption.set_remote_result(remote_result);
			recv_buff.clear();
			exchange_key = false;
		}
		if(remote_result.size() > global::DH_KEY_SIZE){
			LOGGER << " abusive, failed key negotation, too many bytes";
			DB_Blacklist.add(IP);
		}
		return;
	}

	Encryption.crypt_recv(buff, n_bytes);
*/
	recv_buff.append(buff, n_bytes);
	if(recv_buff.size() > global::MAX_MESSAGE_SIZE * global::PIPELINE_SIZE){
		//server sent more than the maximum possible
		LOGGER << "remote host overpipelined (exceeded maximum buffer size) " << IP;
		DB_Blacklist.add(IP);
	}

//DEBUG, break this up in to requests and responses

	//slice off one command at a time and process
	std::string process_send, process_request;
	while(recv_buff.size()){
		process_send.clear();
		process_request.clear();

		if(recv_buff[0] == global:: P_SLOT || recv_buff[0] == global::P_BLOCK
			|| recv_buff[0] == global::P_WAIT || recv_buff[0] == global::P_ERROR)
		{
			if(!recv_buff_read_block()){
				break;
			}
		}else if(recv_buff[0] == global::P_CLOSE_SLOT && recv_buff.size() >= global::P_CLOSE_SLOT_SIZE){
			process_request = recv_buff.substr(0, global::P_CLOSE_SLOT_SIZE);
			recv_buff.erase(0, global::P_CLOSE_SLOT_SIZE);
			close_slot(process_request);
		}else if(recv_buff[0] == global::P_REQUEST_SLOT_HASH_TREE && recv_buff.size() >= global::P_REQUEST_SLOT_HASH_TREE_SIZE){
			process_request = recv_buff.substr(0, global::P_REQUEST_SLOT_HASH_TREE_SIZE);
			recv_buff.erase(0, global::P_REQUEST_SLOT_HASH_TREE_SIZE);
			request_slot_hash(process_request, process_send);
		}else if(recv_buff[0] == global::P_REQUEST_SLOT_FILE && recv_buff.size() >= global::P_REQUEST_SLOT_FILE_SIZE){
			process_request = recv_buff.substr(0, global::P_REQUEST_SLOT_FILE_SIZE);
			recv_buff.erase(0, global::P_REQUEST_SLOT_FILE_SIZE);
			request_slot_file(process_request, process_send);
		}else if(recv_buff[0] == global::P_REQUEST_BLOCK && recv_buff.size() >= global::P_REQUEST_BLOCK_SIZE){
			process_request = recv_buff.substr(0, global::P_REQUEST_BLOCK_SIZE);
			recv_buff.erase(0, global::P_REQUEST_BLOCK_SIZE);
			send_block(process_request, process_send);
		}else{
			//no full command left to slice off
			break;
		}

		//encrypt bytes to send
		//Encryption.crypt_send(process_send);
		send_buff += process_send;
	}

	if(initial_empty && !send_buff.empty()){
		++send_pending();
	}
}

bool p2p_buffer::recv_buff_read_block()
{
	//preconditions
	assert(recv_buff.size() != 0);
	assert(recv_buff[0] == global::P_SLOT || recv_buff[0] == global::P_BLOCK
		|| recv_buff[0] == global::P_WAIT || recv_buff[0] == global::P_ERROR);

	if(Pipeline.empty()){
		LOGGER << "remote host made hash/file request when pipeline empty";
		DB_Blacklist.add(IP);
		return false;
	}

	if(Pipeline.front().Mode == download::BINARY_MODE){
		//find out how many bytes are expected for this command
		std::vector<std::pair<char, int> >::iterator iter_cur, iter_end;
		iter_cur = Pipeline.front().expected.begin();
		iter_end = Pipeline.front().expected.end();
		while(iter_cur != iter_end){
			if(iter_cur->first == recv_buff[0]){
				break;
			}
			++iter_cur;
		}

		if(iter_cur == iter_end){
			//download didn't specify command listed in preconditions
			LOGGER << "download didn't specify a command";
			exit(1);
		}else{
			if(recv_buff.size() < iter_cur->second){
				//not enough bytes yet received to fulfill download's request
				if(Pipeline.front().Download.get() != NULL){
					Pipeline.front().Download->update_speed(socket, recv_buff.size() - bytes_seen);
				}
				bytes_seen = recv_buff.size();
				return false;
			}

			if(Pipeline.front().Download.get() == NULL){
				//terminated download detected, discard response
				recv_buff.erase(0, iter_cur->second);
			}else{
				//pass response to download
				Pipeline.front().Download->response(socket, recv_buff.substr(0, iter_cur->second));
				if(Pipeline.front().Download.get() != NULL){
					Pipeline.front().Download->update_speed(socket, iter_cur->second - bytes_seen);
				}
				bytes_seen = 0;
				recv_buff.erase(0, iter_cur->second);
			}
			Pipeline.pop_front();
		}
		return true;
	}else if(Pipeline.front().Mode == download::TEXT_MODE){
		int loc;
		if((loc = recv_buff.find_first_of('\0',0)) != std::string::npos){
			//pass response to download
			Pipeline.front().Download->response(socket, recv_buff.substr(0, loc));
			recv_buff.erase(0, loc+1);
			Pipeline.front().Download->update_speed(socket, loc);
			Pipeline.pop_front();
			return true;
		}else{
			//whole message not yet received
			return false;
		}
	}

	LOGGER << "invalid mode";
	exit(1);
}

void p2p_buffer::request_slot_hash(const std::string & request, std::string & send)
{
	std::string root_hash = convert::bin_to_hex(request.substr(1,20));

	//check if hash tree currently downloading
	boost::uint64_t file_size;
	if(DB_Download.lookup_hash(root_hash, file_size)){
		int slot_num;
		if(find_empty_slot(root_hash, slot_num)){
			LOGGER << "granting hash slot " << slot_num << " to " << IP;
			Upload_Slot[slot_num] = new slot_hash_tree(&IP, root_hash, file_size);
			send += global::P_SLOT;
			send += (char)slot_num;
		}
		return;
	}

	//check if hash tree exists in share
	if(DB_Share.lookup_hash(root_hash, file_size)){
		int slot_num;
		if(find_empty_slot(root_hash, slot_num)){
			LOGGER << "granting hash slot " << slot_num << " to " << IP;
			Upload_Slot[slot_num] = new slot_hash_tree(&IP, root_hash, file_size);
			send += global::P_SLOT;
			send += (char)slot_num;
		}
		return;
	}

	LOGGER << IP << " requested unavailable hash tree " << root_hash;
	send += global::P_ERROR;
}

void p2p_buffer::request_slot_file(const std::string & request, std::string & send)
{
	std::string root_hash = convert::bin_to_hex(request.substr(1,20));

	//check if file is currently downloading
	boost::uint64_t file_size;
	std::string file_path;
	if(DB_Download.lookup_hash(root_hash, file_path, file_size)){
		int slot_num;
		if(find_empty_slot(root_hash, slot_num)){
			//slot available
			LOGGER << "granting file slot " << slot_num << " to " << IP;
			Upload_Slot[slot_num] = new slot_file(&IP, root_hash, file_path, file_size);
			send += global::P_SLOT;
			send += (char)slot_num;
		}
		return;
	}

	if(DB_Share.lookup_hash(root_hash, file_path, file_size)){
		int slot_num;
		if(find_empty_slot(root_hash, slot_num)){
			//slot available
			LOGGER << "granting file slot " << slot_num << " to " << IP;
			Upload_Slot[slot_num] = new slot_file(&IP, root_hash, file_path, file_size);
			send += global::P_SLOT;
			send += (char)slot_num;
		}
		return;
	}

	LOGGER << IP << " requested unavailable hash tree " << root_hash;
	send += global::P_ERROR;
}

void p2p_buffer::send_block(const std::string & request, std::string & send)
{
	int slot_num = (int)(unsigned char)request[1];
	if(Upload_Slot[slot_num] != NULL){
		//valid slot
		Upload_Slot[slot_num]->send_block(request, send);
	}else{
		LOGGER << IP << " sent invalid slot ID " << slot_num;
		DB_Blacklist.add(IP);
	}
}

void p2p_buffer::uploads(std::vector<upload_info> & UI)
{
	for(int x=0; x<256; ++x){
		if(Upload_Slot[x] != NULL){
			Upload_Slot[x]->info(UI);
		}
	}
}

void p2p_buffer::register_download(boost::shared_ptr<download> new_download)
{
	if(Download.empty()){
		Download.push_back(new_download);
		Download_iter = Download.begin();
	}else{
		Download.push_back(new_download);
	}
}

bool p2p_buffer::rotate_downloads()
{
	if(!Download.empty()){
		++Download_iter;
	}
	if(Download_iter == Download.end()){
		Download_iter = Download.begin();
		return true;
	}
	return false;
}

void p2p_buffer::terminate_download(boost::shared_ptr<download> term_DL)
{
	/*
	If this download is in the Pipeline set the download pointer to NULL to
	indicate that incoming data for the download should be discarded.
	*/
	std::deque<pending_response>::iterator P_iter_cur, P_iter_end;
	P_iter_cur = Pipeline.begin();
	P_iter_end = Pipeline.end();
	while(P_iter_cur != P_iter_end){
		if(P_iter_cur->Download == term_DL){
			P_iter_cur->Download = boost::shared_ptr<download>();
		}
		++P_iter_cur;
	}

	//remove the download
	std::list<boost::shared_ptr<download> >::iterator D_iter_cur, D_iter_end;
	D_iter_cur = Download.begin();
	D_iter_end = Download.end();
	while(D_iter_cur != D_iter_end){
		if(*D_iter_cur == term_DL){
			D_iter_cur = Download.erase(D_iter_cur);
		}else{
			++D_iter_cur;
		}
	}

	//reset Download_iter because it may have been invalidated
	Download_iter = Download.begin();
}
