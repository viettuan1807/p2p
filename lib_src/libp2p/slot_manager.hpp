#ifndef H_SLOT_MANAGER
#define H_SLOT_MANAGER

//custom
#include "exchange.hpp"
#include "share.hpp"
#include "slot.hpp"

//include
#include <boost/thread.hpp>
#include <boost/utility.hpp>
#include <convert.hpp>
#include <network/network.hpp>

//standard
#include <vector>


class slot_manager : private boost::noncopyable
{
public:
	slot_manager(
		exchange & Exchange_in,
		network::connection_info & CI
	);

	/* Functions to receive messages.
	The functions are named after the messages they receive. False is returned if
	the remote host violated the protocol.
	*/
	bool recv_REQUEST_SLOT(boost::shared_ptr<exchange::message> & M);
	bool recv_REQUEST_SLOT_FAILED(boost::shared_ptr<exchange::message> & M);

	/*
	resume:
		When the peer_ID is received this function is called to resume downloads.
	*/
	void resume(const std::string & peer_ID);

private:
	exchange & Exchange;

	//used to detect when slot within share modified
	int share_slot_state;

	/*
	Outgoing_Slot holds slots we have opened with the remote host.
	Incoming_Slot container holds slots the remote host has opened with us.
	Note: Index in vector is slot ID.
	Note: If there is an empty shared_ptr it means the slot ID is available.
	*/
	std::map<unsigned char, boost::shared_ptr<slot> > Outgoing_Slot;
	std::map<unsigned char, boost::shared_ptr<slot> > Incoming_Slot;

	/*
	Pending_Slot_Request:
		Slots which need to be opened with the remote host.
	Slot_Request:
		Slot elements for which we've requested a slot are pushed on the back of
		this. When a response arrives it will be for the element on the front of
		this.
	*/
	std::list<boost::shared_ptr<slot> > Pending_Slot_Request;
	std::list<boost::shared_ptr<slot> > Slot_Request;

	/*
	make_slot_requests:
		Does pending slot requests.
	send_CLOSE_SLOT:
		Send CLOSE_SLOT for specified slot_ID.
	send_REQUEST_SLOT_FAILED:
		Send REQUEST_SLOT_FAILED.
	*/
	void make_slot_requests();
	void send_CLOSE_SLOT(const unsigned char slot_ID);
	void send_REQUEST_SLOT_FAILED();
};
#endif
