#include "exchange_tcp.hpp"

exchange_tcp::exchange_tcp(
	network::proactor & Proactor_in,
	network::connection_info & CI
):
	connection_ID(CI.connection_ID),
	Proactor(Proactor_in),
	blacklist_state(0)
{
	//start key exchange
	if(CI.direction == network::outgoing){
		send(boost::shared_ptr<message_tcp::send::base>(new message_tcp::send::key_exchange_p_rA(
			Encryption.send_p_rA())));
		expect_response(boost::shared_ptr<message_tcp::recv::base>(new message_tcp::recv::key_exchange_rB(
			boost::bind(&exchange_tcp::recv_rB, this, _1, boost::ref(CI)))));
	}else{
		expect_response(boost::shared_ptr<message_tcp::recv::base>(new message_tcp::recv::key_exchange_p_rA(
			boost::bind(&exchange_tcp::recv_p_rA, this, _1, boost::ref(CI)))));
	}
}

void exchange_tcp::expect_response(boost::shared_ptr<message_tcp::recv::base> M)
{
	Expect_Response.push_back(M);
}

void exchange_tcp::expect_anytime(boost::shared_ptr<message_tcp::recv::base> M)
{
	Expect_Anytime.push_back(M);
}

void exchange_tcp::expect_anytime_erase(boost::shared_ptr<message_tcp::send::base> M)
{
	for(std::list<boost::shared_ptr<message_tcp::recv::base> >::iterator
		iter_cur = Expect_Anytime.begin(), iter_end = Expect_Anytime.end();
		iter_cur != iter_end; ++iter_cur)
	{
		if((*iter_cur)->expect(M->buf)){
			/*
			Schedule message to be erased. We don't erase element here because it
			might invalidate iterator in recv loop in exchange_tcp::recv_call_back.
			The control flow is complicated here. We may be calling this function
			from a call back done in exchange_tcp::recv_call_back.
			*/
			*iter_cur = boost::shared_ptr<message_tcp::recv::base>();
		}
	}
}

void exchange_tcp::recv_call_back(network::connection_info & CI)
{
	if(Encryption.ready()){
		Encryption.crypt_recv(CI.recv_buf, CI.recv_buf.size() - CI.latest_recv);
	}

	//process incoming messages
	message_tcp::recv::status Status;
	begin:
	while(!CI.recv_buf.empty()){
		//check if message is expected response
		if(!Expect_Response.empty()){
			if(!Expect_Response.front()){
				Expect_Response.pop_front();
				goto begin;
			}
			Status = Expect_Response.front()->recv(CI.recv_buf);
			if(Status == message_tcp::recv::blacklist){
				db::table::blacklist::add(CI.IP);
				goto end;
			}else if(Status == message_tcp::recv::complete){
				Expect_Response.pop_front();
				goto begin;
			}else if(Status == message_tcp::recv::incomplete){
				goto end;
			}
		}
		//check if message is expected anytime
		for(std::list<boost::shared_ptr<message_tcp::recv::base> >::iterator
			iter_cur = Expect_Anytime.begin(), iter_end = Expect_Anytime.end();
			iter_cur != iter_end;)
		{
			if(*iter_cur){
				Status = (*iter_cur)->recv(CI.recv_buf);
				if(Status == message_tcp::recv::blacklist){
					db::table::blacklist::add(CI.IP);
					goto end;
				}else if(Status == message_tcp::recv::complete){
					goto begin;
				}else if(Status == message_tcp::recv::incomplete){
					goto end;
				}else{
					++iter_cur;
				}
			}else{
				//message sceduled to be erased
				iter_cur = Expect_Anytime.erase(iter_cur);
			}
		}
		//a message was not processed, message on front of recv_buf is not expected
		db::table::blacklist::add(CI.IP);
		goto end;
	}
	end:

	//receive may have caused host to be blacklisted
	if(db::table::blacklist::modified(blacklist_state)
		&& db::table::blacklist::is_blacklisted(CI.IP))
	{
		LOGGER << "disconnecting blacklisted " << CI.IP;
		Proactor.disconnect(connection_ID);
		CI.recv_call_back.clear();
		return;
	}
}

bool exchange_tcp::recv_p_rA(const network::buffer & buf,
	network::connection_info & CI)
{
	if(!Encryption.recv_p_rA(buf)){
		return false;
	}
	send(boost::shared_ptr<message_tcp::send::base>(new message_tcp::send::key_exchange_rB(
		Encryption.send_rB())));
	//unencrypt any remaining buffer
	Encryption.crypt_recv(CI.recv_buf);
	send_buffered();
	return true;
}

bool exchange_tcp::recv_rB(const network::buffer & buf, network::connection_info & CI)
{
	Encryption.recv_rB(buf);
	//unencrypt any remaining buffer
	Encryption.crypt_recv(CI.recv_buf);
	send_buffered();
	return true;
}

void exchange_tcp::send(boost::shared_ptr<message_tcp::send::base> M)
{
	assert(M);
	assert(!M->buf.empty());
	if(!Encryption.ready() && M->encrypt()){
		//buffer messages sent before key exchange complete
		Encrypt_Buf.push_back(M);
	}else if(Encryption.ready() && M->encrypt()){
		//send encrypted message
		Encryption.crypt_send(M->buf);
		Send_Speed.push_back(std::make_pair(M->buf.size(), M->Upload_Speed));
		Proactor.send(connection_ID, M->buf);
	}else if(!M->encrypt()){
		//sent unencrypted message
		Send_Speed.push_back(std::make_pair(M->buf.size(), M->Upload_Speed));
		Proactor.send(connection_ID, M->buf);
	}
}

void exchange_tcp::send_buffered()
{
	for(std::list<boost::shared_ptr<message_tcp::send::base> >::iterator
		iter_cur = Encrypt_Buf.begin(), iter_end = Encrypt_Buf.end();
		iter_cur != iter_end; ++iter_cur)
	{
		send(*iter_cur);
	}
	Encrypt_Buf.clear();
}

void exchange_tcp::send_call_back(network::connection_info & CI)
{
	unsigned latest_send = CI.latest_send;
	while(latest_send){
		assert(!Send_Speed.empty());
		if(latest_send >= Send_Speed.front().first){
			if(Send_Speed.front().second){
				Send_Speed.front().second->add(Send_Speed.front().first);
			}
			latest_send -= Send_Speed.front().first;
			Send_Speed.pop_front();
		}else{
			if(Send_Speed.front().second){
				Send_Speed.front().second->add(latest_send);
			}
			Send_Speed.front().first -= latest_send;
			break;
		}
	}
}