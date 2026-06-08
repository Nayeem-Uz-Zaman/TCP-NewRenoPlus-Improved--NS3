#ifndef TCP_NEWRENO_PLUS_H
#define TCP_NEWRENO_PLUS_H

#include "tcp-congestion-ops.h"
#include "tcp-socket-state.h"
#include "ns3/nstime.h"

namespace ns3
{

class TcpNewRenoPlus : public TcpNewReno
{
  public:
    /**
     * \brief Get the type ID.
     * \return the object TypeId
     */
    static TypeId GetTypeId();

    /**
     * \brief Constructor
     */
    TcpNewRenoPlus();

    /**
     * \brief Copy constructor
     * \param sock the object to copy
     */
    TcpNewRenoPlus(const TcpNewRenoPlus& sock);

    /**
     * \brief Destructor
     */
    ~TcpNewRenoPlus() override;

    /**
     * \brief Get the name of the congestion control algorithm
     * \return A string identifying this congestion control
     */
    std::string GetName() const override;

    /**
     * \brief Get the slow start threshold after a loss event
     * \param tcb internal congestion state
     * \param bytesInFlight bytes in flight
     * \return the slow start threshold value
     */
    uint32_t GetSsThresh(Ptr<const TcpSocketState> tcb,
                         uint32_t bytesInFlight) override;

    /**
     * \brief Set congestion state machine to the correct state
     * \param tcb internal congestion state
     * \param newState new congestion state to which the TCP is going to switch
     */
    void CongestionStateSet(Ptr<TcpSocketState> tcb,
                            const TcpSocketState::TcpCongState_t newState) override;

    /**
     * \brief Update RTT estimate on each ACK
     * \param tcb internal congestion state
     * \param segmentsAcked number of segments acknowledged
     * \param rtt measured RTT sample
     */
    void PktsAcked(Ptr<TcpSocketState> tcb,
                   uint32_t segmentsAcked,
                   const Time& rtt) override;

    /**
     * \brief Increase congestion window; restores any pending cwnd saved in GetSsThresh
     * \param tcb internal congestion state
     * \param segmentsAcked number of segments acknowledged
     */
    void IncreaseWindow(Ptr<TcpSocketState> tcb, uint32_t segmentsAcked) override;

    /**
     * \brief Copy the congestion control algorithm across socket
     * \return a pointer of the copied object
     */
    Ptr<TcpCongestionOps> Fork() override;

  protected:
    // Algorithm state variables
    uint32_t m_tc;           //!< Timeout Counter - cumulative count of timeouts
    uint32_t m_dc;           //!< Dupack Counter - cumulative count of dupack events
    double m_tdr;            //!< Timeout-to-Dupack Ratio: TC / DC
    double m_tdrThreshold;   //!< TDR_T: threshold for TDR (default 0.10)
    double m_tdrAgingFactor; //!< TDR_A: aging factor for TDR EWMA (default 0.875)

    double m_itr;            //!< Interval-to-Time-Difference Ratio: TI / TD
    double m_itrThreshold;   //!< ITR_T: threshold for ITR (default 0.25)
    double m_itrAgingFactor; //!< ITR_A: aging factor for ITR EWMA (default 0.875)

    Time m_latestTimeout;    //!< Timestamp of the latest timeout event
    Time m_rttEstimate;      //!< EWMA RTT estimate used to derive TI (timeout interval)

    uint32_t m_pendingCwnd;    //!< Desired cwnd to restore on next IncreaseWindow call
    bool m_lastLossWasTimeout; //!< True if the most recent loss event was a timeout

  private:
    /**
     * \brief Handle dupack action (Algorithm 1)
     */
    void DupackAction();

    /**
     * \brief Handle timeout action (Algorithm 2)
     */
    void TimeoutAction();

    /**
     * \brief Estimate the retransmission timeout interval from current RTT estimate
     * \return estimated timeout interval (minimum 1 second)
     */
    Time EstimateTimeoutInterval() const;
};

} // namespace ns3

#endif /* TCP_NEWRENO_PLUS_H */
