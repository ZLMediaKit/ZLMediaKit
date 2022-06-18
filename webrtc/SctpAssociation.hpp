#ifndef MS_RTC_SCTP_ASSOCIATION_HPP
#define MS_RTC_SCTP_ASSOCIATION_HPP

#ifdef ENABLE_SCTP
#include <usrsctp.h>
#include "Utils.hpp"
#include "Poller/EventPoller.h"

namespace RTC
{
    class SctpEnv;
    class SctpStreamParameters
    {
    public:
        uint16_t streamId{ 0u };
        bool ordered{ true };
        uint16_t maxPacketLifeTime{ 0u };
        uint16_t maxRetransmits{ 0u };
    };

	class SctpAssociation
	{
	public:
		enum class SctpState
		{
			NEW = 1,
			CONNECTING,
			CONNECTED,
			FAILED,
			CLOSED
		};

	private:
		enum class StreamDirection
		{
			INCOMING = 1,
			OUTGOING
		};

	public:
		class Listener
		{
		public:
			virtual void OnSctpAssociationConnecting(RTC::SctpAssociation* sctpAssociation) = 0;
			virtual void OnSctpAssociationConnected(RTC::SctpAssociation* sctpAssociation)  = 0;
			virtual void OnSctpAssociationFailed(RTC::SctpAssociation* sctpAssociation)     = 0;
			virtual void OnSctpAssociationClosed(RTC::SctpAssociation* sctpAssociation)     = 0;
			virtual void OnSctpAssociationSendData(
			  RTC::SctpAssociation* sctpAssociation, const uint8_t* data, size_t len) = 0;
			virtual void OnSctpAssociationMessageReceived(
			  RTC::SctpAssociation* sctpAssociation,
			  uint16_t streamId,
			  uint32_t ppid,
			  const uint8_t* msg,
			  size_t len) = 0;
		};

	public:
		static bool IsSctp(const uint8_t* data, size_t len)
		{
			// clang-format off
			return (
				(len >= 12) &&
				// Must have Source Port Number and Destination Port Number set to 5000 (hack).
				(Utils::Byte::Get2Bytes(data, 0) == 5000) &&
				(Utils::Byte::Get2Bytes(data, 2) == 5000)
			);
			// clang-format on
		}

	public:
		SctpAssociation(
		  Listener* listener, uint16_t os, uint16_t mis, size_t maxSctpMessageSize, bool isDataChannel);
		virtual ~SctpAssociation();

	public:
		void TransportConnected();
		size_t GetMaxSctpMessageSize() const
		{
			return this->maxSctpMessageSize;
		}
		SctpState GetState() const
		{
			return this->state;
		}
		void ProcessSctpData(const uint8_t* data, size_t len);
		void SendSctpMessage(const RTC::SctpStreamParameters &params, uint32_t ppid, const uint8_t* msg, size_t len);
		void HandleDataConsumer(const RTC::SctpStreamParameters &params);
		void DataProducerClosed(const RTC::SctpStreamParameters &params);
		void DataConsumerClosed(const RTC::SctpStreamParameters &params);

	private:
		void ResetSctpStream(uint16_t streamId, StreamDirection);
		void AddOutgoingStreams(bool force = false);

	public:
        /* Callbacks fired by usrsctp events. */
        virtual void OnUsrSctpSendSctpData(void* buffer, size_t len);
        virtual void OnUsrSctpReceiveSctpData(uint16_t streamId, uint16_t ssn, uint32_t ppid, int flags, const uint8_t* data, size_t len);
        virtual void OnUsrSctpReceiveSctpNotification(union sctp_notification* notification, size_t len);

	private:
		// Passed by argument.
		Listener* listener{ nullptr };
		uint16_t os{ 1024u };
		uint16_t mis{ 1024u };
		size_t maxSctpMessageSize{ 262144u };
		bool isDataChannel{ false };
		// Allocated by this.
		uint8_t* messageBuffer{ nullptr };
		// Others.
		SctpState state{ SctpState::NEW };
		struct socket* socket{ nullptr };
		uint16_t desiredOs{ 0u };
		size_t messageBufferLen{ 0u };
		uint16_t lastSsnReceived{ 0u }; // Valid for us since no SCTP I-DATA support.
        std::shared_ptr<SctpEnv> _env;
	};

    //保证线程安全
    class SctpAssociationImp : public SctpAssociation, public std::enable_shared_from_this<SctpAssociationImp>{
    public:
        using Ptr = std::shared_ptr<SctpAssociationImp>;
        template<typename ... ARGS>
        SctpAssociationImp(toolkit::EventPoller::Ptr poller, ARGS &&...args) : SctpAssociation(std::forward<ARGS>(args)...) {
            _poller = std::move(poller);
        }

        ~SctpAssociationImp() override = default;

    protected:
        void OnUsrSctpSendSctpData(void* buffer, size_t len) override;
        void OnUsrSctpReceiveSctpData(uint16_t streamId, uint16_t ssn, uint32_t ppid, int flags, const uint8_t* data, size_t len) override;
        void OnUsrSctpReceiveSctpNotification(union sctp_notification* notification, size_t len) override;

    private:
        toolkit::EventPoller::Ptr _poller;
    };
} // namespace RTC

#endif //ENABLE_SCTP
#endif //MS_RTC_SCTP_ASSOCIATION_HPP
