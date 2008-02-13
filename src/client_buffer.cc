#include <iostream>

#include "client_buffer.h"
#include "conversion.h"

client_buffer::client_buffer(int socket_in, std::string & server_IP_in, atomic<int> * send_pending_in)
{
	socket = socket_in;
	server_IP = server_IP_in;
	send_pending = send_pending_in;
	recv_buff.reserve(global::C_MAX_SIZE);
	send_buff.reserve(global::S_MAX_SIZE);
	abuse = false;
	last_seen = time(0);
}

client_buffer::~client_buffer()
{
	unreg_all();
}

void client_buffer::add_download(download * new_download)
{
	Download.push_back(new_download);
	Download_iter = Download.begin();
}

const bool client_buffer::empty()
{
	return Download.empty();
}

const std::string & client_buffer::get_IP()
{
	return server_IP;
}

const time_t & client_buffer::get_last_seen()
{
	return last_seen;
}

int client_buffer::post_recv()
{
	//make sure the server didn't respond out of turn, don't try to do Pipeline.front() on an empty Pipeline
	if(Pipeline.empty()){
		#ifdef DEBUG
		std::cout << "error: client_buffer::post_recv() set abuse flag 1 on " << server_IP << "\n";
		#endif
		abuse = true;
		return 0;
	}

	while(recv_buff.size() >= Pipeline.front().first){
		Pipeline.front().second->response(socket, recv_buff.substr(0, Pipeline.front().first));
		recv_buff.erase(0, Pipeline.front().first);
		Pipeline.pop_front();

		if(Pipeline.empty()){
			break;
		}
	}

	//if recv_buff larger than largest case the server is doing something naughty
	if(recv_buff.size() > global::PIPELINE_SIZE * global::C_MAX_SIZE){
		#ifdef DEBUG
		std::cout << "error: client_buffer::post_recv() set abuse flag 2 on " << server_IP << "\n";
		#endif
		abuse = true;
	}

	last_seen = time(0);
}

void client_buffer::post_send()
{
	if(send_buff.empty()){
		--(*send_pending);
	}
}

void client_buffer::prepare_request()
{
	std::string request;
	int count = 0;
	if(!Download.empty()){
		while(Pipeline.size() <= global::PIPELINE_SIZE && ++count <= global::PIPELINE_SIZE){
			rotate_downloads();
			if((*Download_iter)->request(socket, request)){
				send_buff += request;
				Pipeline.push_back(std::make_pair((*Download_iter)->bytes_expected(),*Download_iter));
			}
		}
		if(send_buff.size() != 0){
			++(*send_pending);
		}
	}
}

void client_buffer::rotate_downloads()
{
	if(!Download.empty()){
		++Download_iter;
	}

	if(Download_iter == Download.end()){
		Download_iter = Download.begin();
	}
}

const bool client_buffer::terminate_download(const std::string & hash)
{
	/*
	Case 1:
	The download was put in the Pending_Termination container because there were still
	bytes expected for it. Check to see if still expecting bytes, if not then terminate
	the download.
	*/
	std::list<download *>::iterator PT_iter_cur, PT_iter_end;
	PT_iter_cur = Pending_Termination.begin();
	PT_iter_end = Pending_Termination.end();
	while(PT_iter_cur != PT_iter_end){
		if(hash == (*PT_iter_cur)->hash()){
			//download already in progress of termination, check pipeline
			std::deque<std::pair<int, download *> >::iterator P_iter_cur, P_iter_end;
			P_iter_cur = Pipeline.begin();
			P_iter_end = Pipeline.end();
			while(P_iter_cur != P_iter_end){
				if(hash == P_iter_cur->second->hash()){
					//download found in pipeline, do not terminate yet
					return false;
				}
				++P_iter_cur;
			}
			//download not found in Pipeline, terminate
			(*PT_iter_cur)->unreg_conn(socket);
			Pending_Termination.erase(PT_iter_cur);
			return true;
		}
		++PT_iter_cur;
	}

	/*
	Case 2:
	The download is not in Pending_Termination so this is the first attempt to terminate
	the download. Find the download and check to see if it is in the Pipeline. If the download
	is in the Pipeline then move it to Pending_Termination and erase if from the Download
	container. If the download is not in the Pipeline then delete it from Download.
	*/
	std::list<download *>::iterator D_iter_cur, D_iter_end;
	D_iter_cur = Download.begin();
	D_iter_end = Download.end();
	while(D_iter_cur != D_iter_end){
		if(hash == (*D_iter_cur)->hash()){
			//check to see if download is in the Pipeline
			std::deque<std::pair<int, download *> >::iterator P_iter_cur, P_iter_end;
			P_iter_cur = Pipeline.begin();
			P_iter_end = Pipeline.end();
			while(P_iter_cur != P_iter_end){
				if(hash == P_iter_cur->second->hash()){
					//download found in pipeline
					Pending_Termination.push_back(*D_iter_cur);
					Download.erase(D_iter_cur);
					Download_iter = Download.begin(); //iterator may have been invalidated
					return false;
				}
				++P_iter_cur;
			}

			//download not found in Pipeline, terminate
			(*D_iter_cur)->unreg_conn(socket);
			Download.erase(D_iter_cur);
			Download_iter = Download.begin(); //iterator may have been invalidated
			return true;
		}
		++D_iter_cur;
	}

	//the download is not registered with this client_buffer
	return true;
}

void client_buffer::unreg_all()
{
	std::list<download *>::iterator iter_cur, iter_end;
	iter_cur = Download.begin();
	iter_end = Download.end();
	while(iter_cur != iter_end){
		(*iter_cur)->unreg_conn(socket);
		++iter_cur;
	}
}

