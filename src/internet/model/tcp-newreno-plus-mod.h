/*
 * tcp-newreno-plus-mod.h — Three-Factor Modified NewReno+
 *
 * Replaces the original NewReno+ TDR metric with:
 *   1. Congestion Probability  CP  = TC / (TC + DC)         [EWMA-smoothed, bounded 0–1]
 *   2. Normalized Queueing Delay NQD = (RTTcurr − RTTmin) / RTTcurr  [bounded 0–1]
 * and retains the original ITR metric (TI / TD, EWMA-smoothed).
 *
 * Three-level decision on every loss event:
 *   Level 1 (AND — all three say wireless):  ssthresh = cwnd = 0.85 × cwnd
 *   Level 2 (OR  — any one says wireless):   ssthresh = cwnd = 0.75 × cwnd
 *   Level 3 (none — congestion):             ssthresh = cwnd = 0.50 × cwnd
 */

#ifndef TCP_NEWRENO_PLUS_MOD_H
#define TCP_NEWRENO_PLUS_MOD_H

#include "tcp-congestion-ops.h"
#include "tcp-socket-state.h"

#include "ns3/nstime.h"

namespace ns3
{

/**
 * \ingroup congestionOps
 * \brief Three-factor modified NewReno+ congestion control
 *
 * Independent sibling of TcpNewRenoPlus — both inherit directly from TcpNewReno.
 */
class TcpNewRenoPlusMod : public TcpNewReno
{
  public:
    static TypeId GetTypeId();

    TcpNewRenoPlusMod();
    TcpNewRenoPlusMod(const TcpNewRenoPlusMod& sock);
    ~TcpNewRenoPlusMod() override;

    std::string GetName() const override;
    Ptr<TcpCongestionOps> Fork() override;

    uint32_t GetSsThresh(Ptr<const TcpSocketState> tcb,
                         uint32_t bytesInFlight) override;

    void CongestionStateSet(Ptr<TcpSocketState> tcb,
                            const TcpSocketState::TcpCongState_t newState) override;

    void PktsAcked(Ptr<TcpSocketState> tcb,
                   uint32_t segmentsAcked,
                   const Time& rtt) override;

    void IncreaseWindow(Ptr<TcpSocketState> tcb, uint32_t segmentsAcked) override;

  private:
    // ── helpers ──────────────────────────────────────────────────────────
    void DupackAction();
    void TimeoutAction();
    Time EstimateTimeoutInterval() const;

    // ── loss-event counters ─────────────────────────────────────────────
    uint32_t m_tc;  //!< cumulative timeout count
    uint32_t m_dc;  //!< cumulative dupack-loss count

    // ── Modification 1: Congestion Probability (replaces TDR) ───────────
    double m_cp;           //!< EWMA of CP = TC/(TC+DC), init 0.5 (neutral)
    double m_cpThreshold;  //!< classification boundary (default 0.30)
    double m_cpAging;      //!< EWMA weight on old value  (default 0.875)

    // ── Modification 2: Normalized Queueing Delay ───────────────────────
    double m_nqd;           //!< latest NQD = (RTTcurr − RTTmin)/RTTcurr
    double m_nqdThreshold;  //!< classification boundary (default 0.25)
    Time   m_rttMin;        //!< running minimum RTT (with 0.99/0.01 decay)
    Time   m_rttCurrent;    //!< most recent raw RTT sample

    // ── Original ITR metric (kept from paper) ───────────────────────────
    double m_itr;           //!< EWMA of TI/TD ratio
    double m_itrThreshold;  //!< classification boundary (default 0.25)
    double m_itrAging;      //!< EWMA weight on old value  (default 0.875)
    Time   m_latestTimeout; //!< timestamp of most recent RTO
    Time   m_rttEstimate;   //!< EWMA RTT used for TI = 3×RTT

    // ── cwnd restore after timeout ──────────────────────────────────────
    uint32_t m_pendingCwnd;      //!< desired cwnd saved for IncreaseWindow
    bool     m_lastLossWasTimeout; //!< true ⟹ last loss was RTO, not dupack
};

} // namespace ns3

#endif /* TCP_NEWRENO_PLUS_MOD_H */
