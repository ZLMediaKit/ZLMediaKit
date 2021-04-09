/**
ISC License

Copyright © 2015, Iñaki Baz Castillo <ibc@aliax.net>

Permission to use, copy, modify, and/or distribute this software for any
purpose with or without fee is hereby granted, provided that the above
copyright notice and this permission notice appear in all copies.

THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#ifndef MS_RTC_ICE_SERVER_HPP
#define MS_RTC_ICE_SERVER_HPP

#include "StunPacket.hpp"
#include "logger.h"
#include "Utils.hpp"
#include <list>
#include <string>
#include <functional>
#include <memory>

using _TransportTuple = struct sockaddr;

namespace RTC
{
    using TransportTuple = _TransportTuple;
	class IceServer
	{
	public:
		enum class IceState
		{
			NEW = 1,
			CONNECTED,
			COMPLETED,
			DISCONNECTED
		};

	public:
		class Listener
		{
		public:
			virtual ~Listener() = default;

		public:
			/**
			 * These callbacks are guaranteed to be called before ProcessStunPacket()
			 * returns, so the given pointers are still usable.
			 */
			virtual void OnIceServerSendStunPacket(
			  const RTC::IceServer* iceServer, const RTC::StunPacket* packet, RTC::TransportTuple* tuple) = 0;
			virtual void OnIceServerSelectedTuple(
			  const RTC::IceServer* iceServer, RTC::TransportTuple* tuple)        = 0;
			virtual void OnIceServerConnected(const RTC::IceServer* iceServer)    = 0;
			virtual void OnIceServerCompleted(const RTC::IceServer* iceServer)    = 0;
			virtual void OnIceServerDisconnected(const RTC::IceServer* iceServer) = 0;
		};

	public:
		IceServer(Listener* listener, const std::string& usernameFragment, const std::string& password);

	public:
		void ProcessStunPacket(RTC::StunPacket* packet, RTC::TransportTuple* tuple);
		const std::string& GetUsernameFragment() const
		{
			return this->usernameFragment;
		}
		const std::string& GetPassword() const
		{
			return this->password;
		}
		IceState GetState() const
		{
			return this->state;
		}
		RTC::TransportTuple* GetSelectedTuple() const
		{
			return this->selectedTuple;
		}
		void SetUsernameFragment(const std::string& usernameFragment)
		{
			this->oldUsernameFragment = this->usernameFragment;
			this->usernameFragment    = usernameFragment;
		}
		void SetPassword(const std::string& password)
		{
			this->oldPassword = this->password;
			this->password    = password;
		}
		bool IsValidTuple(const RTC::TransportTuple* tuple) const;
		void RemoveTuple(RTC::TransportTuple* tuple);
		// This should be just called in 'connected' or completed' state
		// and the given tuple must be an already valid tuple.
		void ForceSelectedTuple(const RTC::TransportTuple* tuple);

	private:
		void HandleTuple(RTC::TransportTuple* tuple, bool hasUseCandidate);
		/**
		 * Store the given tuple and return its stored address.
		 */
		RTC::TransportTuple* AddTuple(RTC::TransportTuple* tuple);
		/**
		 * If the given tuple exists return its stored address, nullptr otherwise.
		 */
		RTC::TransportTuple* HasTuple(const RTC::TransportTuple* tuple) const;
		/**
		 * Set the given tuple as the selected tuple.
		 * NOTE: The given tuple MUST be already stored within the list.
		 */
		void SetSelectedTuple(RTC::TransportTuple* storedTuple);

	private:
		// Passed by argument.
		Listener* listener{ nullptr };
		// Others.
		std::string usernameFragment;
		std::string password;
		std::string oldUsernameFragment;
		std::string oldPassword;
		IceState state{ IceState::NEW };
		std::list<RTC::TransportTuple> tuples;
		RTC::TransportTuple* selectedTuple{ nullptr };
		//最大不超过mtu
        static constexpr size_t StunSerializeBufferSize{ 1600 };
        uint8_t StunSerializeBuffer[StunSerializeBufferSize];
	};
} // namespace RTC

#endif
