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

#define MS_CLASS "RTC::IceServer"
// #define MS_LOG_DEV_LEVEL 3

#include <utility>
#include "IceServer.hpp"

namespace RTC
{
	/* Static. */
	/* Instance methods. */

	IceServer::IceServer(Listener* listener, const std::string& usernameFragment, const std::string& password)
	  : listener(listener), usernameFragment(usernameFragment), password(password)
	{
		MS_TRACE();
	}

	void IceServer::ProcessStunPacket(RTC::StunPacket* packet, RTC::TransportTuple* tuple)
	{
		MS_TRACE();

		// Must be a Binding method.
		if (packet->GetMethod() != RTC::StunPacket::Method::BINDING)
		{
			if (packet->GetClass() == RTC::StunPacket::Class::REQUEST)
			{
				MS_WARN_TAG(
				  ice,
				  "unknown method %#.3x in STUN Request => 400",
				  static_cast<unsigned int>(packet->GetMethod()));

				// Reply 400.
				RTC::StunPacket* response = packet->CreateErrorResponse(400);

				response->Serialize(StunSerializeBuffer);
				this->listener->OnIceServerSendStunPacket(this, response, tuple);

				delete response;
			}
			else
			{
				MS_WARN_TAG(
				  ice,
				  "ignoring STUN Indication or Response with unknown method %#.3x",
				  static_cast<unsigned int>(packet->GetMethod()));
			}

			return;
		}

		// Must use FINGERPRINT (optional for ICE STUN indications).
		if (!packet->HasFingerprint() && packet->GetClass() != RTC::StunPacket::Class::INDICATION)
		{
			if (packet->GetClass() == RTC::StunPacket::Class::REQUEST)
			{
				MS_WARN_TAG(ice, "STUN Binding Request without FINGERPRINT => 400");

				// Reply 400.
				RTC::StunPacket* response = packet->CreateErrorResponse(400);

				response->Serialize(StunSerializeBuffer);
				this->listener->OnIceServerSendStunPacket(this, response, tuple);

				delete response;
			}
			else
			{
				MS_WARN_TAG(ice, "ignoring STUN Binding Response without FINGERPRINT");
			}

			return;
		}

		switch (packet->GetClass())
		{
			case RTC::StunPacket::Class::REQUEST:
			{
				// USERNAME, MESSAGE-INTEGRITY and PRIORITY are required.
				if (!packet->HasMessageIntegrity() || (packet->GetPriority() == 0u) || packet->GetUsername().empty())
				{
					MS_WARN_TAG(ice, "mising required attributes in STUN Binding Request => 400");

					// Reply 400.
					RTC::StunPacket* response = packet->CreateErrorResponse(400);

					response->Serialize(StunSerializeBuffer);
					this->listener->OnIceServerSendStunPacket(this, response, tuple);

					delete response;

					return;
				}

				// Check authentication.
				switch (packet->CheckAuthentication(this->usernameFragment, this->password))
				{
					case RTC::StunPacket::Authentication::OK:
					{
						if (!this->oldPassword.empty())
						{
							MS_DEBUG_TAG(ice, "new ICE credentials applied");

							this->oldUsernameFragment.clear();
							this->oldPassword.clear();
						}

						break;
					}

					case RTC::StunPacket::Authentication::UNAUTHORIZED:
					{
						// We may have changed our usernameFragment and password, so check
						// the old ones.
						// clang-format off
						if (
							!this->oldUsernameFragment.empty() &&
							!this->oldPassword.empty() &&
							packet->CheckAuthentication(this->oldUsernameFragment, this->oldPassword) == RTC::StunPacket::Authentication::OK
						)
						// clang-format on
						{
							MS_DEBUG_TAG(ice, "using old ICE credentials");

							break;
						}

						MS_WARN_TAG(ice, "wrong authentication in STUN Binding Request => 401");

						// Reply 401.
						RTC::StunPacket* response = packet->CreateErrorResponse(401);

						response->Serialize(StunSerializeBuffer);
						this->listener->OnIceServerSendStunPacket(this, response, tuple);

						delete response;

						return;
					}

					case RTC::StunPacket::Authentication::BAD_REQUEST:
					{
						MS_WARN_TAG(ice, "cannot check authentication in STUN Binding Request => 400");

						// Reply 400.
						RTC::StunPacket* response = packet->CreateErrorResponse(400);

						response->Serialize(StunSerializeBuffer);
						this->listener->OnIceServerSendStunPacket(this, response, tuple);

						delete response;

						return;
					}
				}

#if 0
				// The remote peer must be ICE controlling.
				if (packet->GetIceControlled())
				{
					MS_WARN_TAG(ice, "peer indicates ICE-CONTROLLED in STUN Binding Request => 487");

					// Reply 487 (Role Conflict).
					RTC::StunPacket* response = packet->CreateErrorResponse(487);

					response->Serialize(StunSerializeBuffer);
					this->listener->OnIceServerSendStunPacket(this, response, tuple);

					delete response;

					return;
				}

#endif

				//MS_DEBUG_DEV(
				//  "processing STUN Binding Request [Priority:%" PRIu32 ", UseCandidate:%s]",
				//  static_cast<uint32_t>(packet->GetPriority()),
				//  packet->HasUseCandidate() ? "true" : "false");

				// Create a success response.
				RTC::StunPacket* response = packet->CreateSuccessResponse();

				// Add XOR-MAPPED-ADDRESS.
				response->SetXorMappedAddress(tuple);

				// Authenticate the response.
				if (this->oldPassword.empty())
					response->Authenticate(this->password);
				else
					response->Authenticate(this->oldPassword);

				// Send back.
				response->Serialize(StunSerializeBuffer);
				this->listener->OnIceServerSendStunPacket(this, response, tuple);

				delete response;

				// Handle the tuple.
				HandleTuple(tuple, packet->HasUseCandidate());

				break;
			}

			case RTC::StunPacket::Class::INDICATION:
			{
				MS_DEBUG_TAG(ice, "STUN Binding Indication processed");

				break;
			}

			case RTC::StunPacket::Class::SUCCESS_RESPONSE:
			{
				MS_DEBUG_TAG(ice, "STUN Binding Success Response processed");

				break;
			}

			case RTC::StunPacket::Class::ERROR_RESPONSE:
			{
				MS_DEBUG_TAG(ice, "STUN Binding Error Response processed");

				break;
			}
		}
	}

	bool IceServer::IsValidTuple(const RTC::TransportTuple* tuple) const
	{
		MS_TRACE();

		return HasTuple(tuple) != nullptr;
	}

	void IceServer::RemoveTuple(RTC::TransportTuple* tuple)
	{
		MS_TRACE();

		RTC::TransportTuple* removedTuple{ nullptr };

		// Find the removed tuple.
		auto it = this->tuples.begin();

		for (; it != this->tuples.end(); ++it)
		{
			RTC::TransportTuple* storedTuple = std::addressof(*it);

			if (memcmp(storedTuple, tuple, sizeof (RTC::TransportTuple)) == 0)
			{
				removedTuple = storedTuple;

				break;
			}
		}

		// If not found, ignore.
		if (!removedTuple)
			return;

		// Remove from the list of tuples.
		this->tuples.erase(it);

		// If this is not the selected tuple, stop here.
		if (removedTuple != this->selectedTuple)
			return;

		// Otherwise this was the selected tuple.
		this->selectedTuple = nullptr;

		// Mark the first tuple as selected tuple (if any).
		if (this->tuples.begin() != this->tuples.end())
		{
			SetSelectedTuple(std::addressof(*this->tuples.begin()));
		}
		// Or just emit 'disconnected'.
		else
		{
			// Update state.
			this->state = IceState::DISCONNECTED;
			// Notify the listener.
			this->listener->OnIceServerDisconnected(this);
		}
	}

	void IceServer::ForceSelectedTuple(const RTC::TransportTuple* tuple)
	{
		MS_TRACE();

		MS_ASSERT(
		  this->selectedTuple, "cannot force the selected tuple if there was not a selected tuple");

		auto* storedTuple = HasTuple(tuple);

		MS_ASSERT(
		  storedTuple,
		  "cannot force the selected tuple if the given tuple was not already a valid tuple");

		// Mark it as selected tuple.
		SetSelectedTuple(storedTuple);
	}

	void IceServer::HandleTuple(RTC::TransportTuple* tuple, bool hasUseCandidate)
	{
		MS_TRACE();

		switch (this->state)
		{
			case IceState::NEW:
			{
				// There should be no tuples.
				MS_ASSERT(
				  this->tuples.empty(), "state is 'new' but there are %zu tuples", this->tuples.size());

				// There shouldn't be a selected tuple.
				MS_ASSERT(!this->selectedTuple, "state is 'new' but there is selected tuple");

				if (!hasUseCandidate)
				{
					MS_DEBUG_TAG(ice, "transition from state 'new' to 'connected'");

					// Store the tuple.
					auto* storedTuple = AddTuple(tuple);

					// Mark it as selected tuple.
					SetSelectedTuple(storedTuple);
					// Update state.
					this->state = IceState::CONNECTED;
					// Notify the listener.
					this->listener->OnIceServerConnected(this);
				}
				else
				{
					MS_DEBUG_TAG(ice, "transition from state 'new' to 'completed'");

					// Store the tuple.
					auto* storedTuple = AddTuple(tuple);

					// Mark it as selected tuple.
					SetSelectedTuple(storedTuple);
					// Update state.
					this->state = IceState::COMPLETED;
					// Notify the listener.
					this->listener->OnIceServerCompleted(this);
				}

				break;
			}

			case IceState::DISCONNECTED:
			{
				// There should be no tuples.
				MS_ASSERT(
				  this->tuples.empty(),
				  "state is 'disconnected' but there are %zu tuples",
				  this->tuples.size());

				// There shouldn't be a selected tuple.
				MS_ASSERT(!this->selectedTuple, "state is 'disconnected' but there is selected tuple");

				if (!hasUseCandidate)
				{
					MS_DEBUG_TAG(ice, "transition from state 'disconnected' to 'connected'");

					// Store the tuple.
					auto* storedTuple = AddTuple(tuple);

					// Mark it as selected tuple.
					SetSelectedTuple(storedTuple);
					// Update state.
					this->state = IceState::CONNECTED;
					// Notify the listener.
					this->listener->OnIceServerConnected(this);
				}
				else
				{
					MS_DEBUG_TAG(ice, "transition from state 'disconnected' to 'completed'");

					// Store the tuple.
					auto* storedTuple = AddTuple(tuple);

					// Mark it as selected tuple.
					SetSelectedTuple(storedTuple);
					// Update state.
					this->state = IceState::COMPLETED;
					// Notify the listener.
					this->listener->OnIceServerCompleted(this);
				}

				break;
			}

			case IceState::CONNECTED:
			{
				// There should be some tuples.
				MS_ASSERT(!this->tuples.empty(), "state is 'connected' but there are no tuples");

				// There should be a selected tuple.
				MS_ASSERT(this->selectedTuple, "state is 'connected' but there is not selected tuple");

				if (!hasUseCandidate)
				{
					// If a new tuple store it.
					if (!HasTuple(tuple))
						AddTuple(tuple);
				}
				else
				{
					MS_DEBUG_TAG(ice, "transition from state 'connected' to 'completed'");

					auto* storedTuple = HasTuple(tuple);

					// If a new tuple store it.
					if (!storedTuple)
						storedTuple = AddTuple(tuple);

					// Mark it as selected tuple.
					SetSelectedTuple(storedTuple);
					// Update state.
					this->state = IceState::COMPLETED;
					// Notify the listener.
					this->listener->OnIceServerCompleted(this);
				}

				break;
			}

			case IceState::COMPLETED:
			{
				// There should be some tuples.
				MS_ASSERT(!this->tuples.empty(), "state is 'completed' but there are no tuples");

				// There should be a selected tuple.
				MS_ASSERT(this->selectedTuple, "state is 'completed' but there is not selected tuple");

				if (!hasUseCandidate)
				{
					// If a new tuple store it.
					if (!HasTuple(tuple))
						AddTuple(tuple);
				}
				else
				{
					auto* storedTuple = HasTuple(tuple);

					// If a new tuple store it.
					if (!storedTuple)
						storedTuple = AddTuple(tuple);

					// Mark it as selected tuple.
					SetSelectedTuple(storedTuple);
				}

				break;
			}
		}
	}

	inline RTC::TransportTuple* IceServer::AddTuple(RTC::TransportTuple* tuple)
	{
		MS_TRACE();

		// Add the new tuple at the beginning of the list.
		this->tuples.push_front(*tuple);

		auto* storedTuple = std::addressof(*this->tuples.begin());

		// Return the address of the inserted tuple.
		return storedTuple;
	}

	inline RTC::TransportTuple* IceServer::HasTuple(const RTC::TransportTuple* tuple) const
	{
		MS_TRACE();

		// If there is no selected tuple yet then we know that the tuples list
		// is empty.
		if (!this->selectedTuple)
			return nullptr;

		// Check the current selected tuple.
		if (memcmp(selectedTuple, tuple, sizeof (RTC::TransportTuple)) == 0)
			return this->selectedTuple;

		// Otherwise check other stored tuples.
		for (const auto& it : this->tuples)
		{
			auto* storedTuple = const_cast<RTC::TransportTuple*>(std::addressof(it));

			if (memcmp(storedTuple, tuple, sizeof (RTC::TransportTuple)) == 0)
				return storedTuple;
		}

		return nullptr;
	}

	inline void IceServer::SetSelectedTuple(RTC::TransportTuple* storedTuple)
	{
		MS_TRACE();

		// If already the selected tuple do nothing.
		if (storedTuple == this->selectedTuple)
			return;

		this->selectedTuple = storedTuple;

		// Notify the listener.
		this->listener->OnIceServerSelectedTuple(this, this->selectedTuple);
	}
} // namespace RTC
