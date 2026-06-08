/*
 * tcp-newreno-plus-mod.cc — Three-Factor Modified NewReno+
 */

#include "tcp-newreno-plus-mod.h"

#include "ns3/log.h"
#include "ns3/simulator.h"

#include <algorithm>

namespace ns3
{

NS_LOG_COMPONENT_DEFINE("TcpNewRenoPlusMod");
NS_OBJECT_ENSURE_REGISTERED(TcpNewRenoPlusMod);


TypeId
TcpNewRenoPlusMod::GetTypeId()
{
    static TypeId tid =
        TypeId("ns3::TcpNewRenoPlusMod")
            .SetParent<TcpNewReno>()
            .SetGroupName("Internet")
            .AddConstructor<TcpNewRenoPlusMod>()
            .AddAttribute("CpThreshold",
                          "CP threshold for wireless-error classification",
                          DoubleValue(0.30),
                          MakeDoubleAccessor(&TcpNewRenoPlusMod::m_cpThreshold),
                          MakeDoubleChecker<double>(0.0, 1.0))
            .AddAttribute("CpAging",
                          "EWMA aging factor for CP (weight on old value)",
                          DoubleValue(0.875),
                          MakeDoubleAccessor(&TcpNewRenoPlusMod::m_cpAging),
                          MakeDoubleChecker<double>(0.0, 1.0))
            .AddAttribute("NqdThreshold",
                          "NQD threshold for queueing-delay classification",
                          DoubleValue(0.25),
                          MakeDoubleAccessor(&TcpNewRenoPlusMod::m_nqdThreshold),
                          MakeDoubleChecker<double>(0.0, 1.0))
            .AddAttribute("ItrThreshold",
                          "ITR threshold for interval-ratio classification",
                          DoubleValue(0.25),
                          MakeDoubleAccessor(&TcpNewRenoPlusMod::m_itrThreshold),
                          MakeDoubleChecker<double>(0.0, 1.0))
            .AddAttribute("ItrAging",
                          "EWMA aging factor for ITR (weight on old value)",
                          DoubleValue(0.875),
                          MakeDoubleAccessor(&TcpNewRenoPlusMod::m_itrAging),
                          MakeDoubleChecker<double>(0.0, 1.0));
    return tid;
}

// ═══════════════════════════════════════════════════════════════════════════
// Constructors / destructor
// ═══════════════════════════════════════════════════════════════════════════

TcpNewRenoPlusMod::TcpNewRenoPlusMod()
    : TcpNewReno(),
      m_tc(0),
      m_dc(0),
      m_cp(0.5),
      m_cpThreshold(0.30),
      m_cpAging(0.875),
      m_nqd(0.0),
      m_nqdThreshold(0.25),
      m_rttMin(Seconds(0)),
      m_rttCurrent(Seconds(0)),
      m_itr(0.0),
      m_itrThreshold(0.25),
      m_itrAging(0.875),
      m_latestTimeout(Seconds(0)),
      m_rttEstimate(Seconds(0)),
      m_pendingCwnd(0),
      m_lastLossWasTimeout(false)
{
    NS_LOG_FUNCTION(this);
}

TcpNewRenoPlusMod::TcpNewRenoPlusMod(const TcpNewRenoPlusMod& sock)
    : TcpNewReno(sock),
      m_tc(sock.m_tc),
      m_dc(sock.m_dc),
      m_cp(sock.m_cp),
      m_cpThreshold(sock.m_cpThreshold),
      m_cpAging(sock.m_cpAging),
      m_nqd(sock.m_nqd),
      m_nqdThreshold(sock.m_nqdThreshold),
      m_rttMin(sock.m_rttMin),
      m_rttCurrent(sock.m_rttCurrent),
      m_itr(sock.m_itr),
      m_itrThreshold(sock.m_itrThreshold),
      m_itrAging(sock.m_itrAging),
      m_latestTimeout(sock.m_latestTimeout),
      m_rttEstimate(sock.m_rttEstimate),
      m_pendingCwnd(sock.m_pendingCwnd),
      m_lastLossWasTimeout(sock.m_lastLossWasTimeout)
{
    NS_LOG_FUNCTION(this);
}

TcpNewRenoPlusMod::~TcpNewRenoPlusMod()
{
    NS_LOG_FUNCTION(this);
}

// ═══════════════════════════════════════════════════════════════════════════
// Identity helpers
// ═══════════════════════════════════════════════════════════════════════════

std::string
TcpNewRenoPlusMod::GetName() const
{
    return "TcpNewRenoPlusMod";
}

Ptr<TcpCongestionOps>
TcpNewRenoPlusMod::Fork()
{
    return CopyObject<TcpNewRenoPlusMod>(this);
}

// ═══════════════════════════════════════════════════════════════════════════
// PktsAcked — RTT tracking + NQD update
// ═══════════════════════════════════════════════════════════════════════════

void
TcpNewRenoPlusMod::PktsAcked(Ptr<TcpSocketState> tcb,
                              uint32_t segmentsAcked,
                              const Time& rtt)
{
    NS_LOG_FUNCTION(this << tcb << segmentsAcked << rtt);

    if (rtt.IsZero())
    {
        return;
    }

    // ── 1. EWMA RTT estimate (α = 0.875 old, 0.125 new) ──
    if (m_rttEstimate.IsZero())
    {
        m_rttEstimate = rtt;
    }
    else
    {
        m_rttEstimate = m_rttEstimate * 0.875 + rtt * 0.125;
    }

    // ── 2. Store current raw sample ──
    m_rttCurrent = rtt;

    // ── 3. RTTmin with slow decay to avoid permanently stuck floor ──
    if (m_rttMin.IsZero() || rtt < m_rttMin)
    {
        m_rttMin = rtt;
    }
    else
    {
        // Slow drift towards current samples: 0.99 × old + 0.01 × new
        m_rttMin = m_rttMin * 0.99 + rtt * 0.01;
    }

    // ── 4. Compute NQD and clamp to [0, 1] ──
    if (m_rttCurrent.GetSeconds() > 0.0)
    {
        double diff = (m_rttCurrent - m_rttMin).GetSeconds();
        m_nqd = diff / m_rttCurrent.GetSeconds();
        m_nqd = std::max(0.0, std::min(1.0, m_nqd));
    }
    else
    {
        m_nqd = 0.0;
    }

    NS_LOG_INFO("PktsAcked: rtt=" << rtt.GetSeconds()
                << "s rttEst=" << m_rttEstimate.GetSeconds()
                << "s rttMin=" << m_rttMin.GetSeconds()
                << "s NQD=" << m_nqd);
}

// ═══════════════════════════════════════════════════════════════════════════
// EstimateTimeoutInterval — TI = max(3×RTT, 1 s)
// ═══════════════════════════════════════════════════════════════════════════

Time
TcpNewRenoPlusMod::EstimateTimeoutInterval() const
{
    if (m_rttEstimate.IsZero())
    {
        return Seconds(1.0);
    }
    return std::max(m_rttEstimate * 3, Seconds(1.0));
}

// ═══════════════════════════════════════════════════════════════════════════
// DupackAction — Algorithm 1 (on CA_RECOVERY)
// ═══════════════════════════════════════════════════════════════════════════

void
TcpNewRenoPlusMod::DupackAction()
{
    NS_LOG_FUNCTION(this);

    // 1. Increment dupack counter
    m_dc++;

    // 2–3. Compute raw CP and EWMA-update m_cp
    double rawCp = ((m_tc + m_dc) > 0)
                       ? static_cast<double>(m_tc) / (m_tc + m_dc)
                       : 0.5;
    m_cp = m_cpAging * m_cp + (1.0 - m_cpAging) * rawCp;

    // 4–7. ITR update: TI / TD
    Time td = Simulator::Now() - m_latestTimeout;
    Time ti = EstimateTimeoutInterval();

    double rawItr;
    if (td.GetSeconds() > 0.0)
    {
        rawItr = ti.GetSeconds() / td.GetSeconds();
    }
    else
    {
        rawItr = 0.01; // no prior timeout — assume non-congestion
    }
    m_itr = m_itrAging * m_itr + (1.0 - m_itrAging) * rawItr;

    // 8. Flag
    m_lastLossWasTimeout = false;

    NS_LOG_INFO("DupackAction: TC=" << m_tc << " DC=" << m_dc
                << " CP=" << m_cp << " ITR=" << m_itr
                << " NQD=" << m_nqd
                << " TI=" << ti.GetSeconds() << "s TD=" << td.GetSeconds() << "s");
}

// ═══════════════════════════════════════════════════════════════════════════
// TimeoutAction — Algorithm 2 (on CA_LOSS)
// ═══════════════════════════════════════════════════════════════════════════

void
TcpNewRenoPlusMod::TimeoutAction()
{
    NS_LOG_FUNCTION(this);

    // 1. Increment timeout counter
    m_tc++;

    // 2–3. Compute raw CP and EWMA-update m_cp
    double rawCp = ((m_tc + m_dc) > 0)
                       ? static_cast<double>(m_tc) / (m_tc + m_dc)
                       : 0.5;
    m_cp = m_cpAging * m_cp + (1.0 - m_cpAging) * rawCp;

    // 4. DC decay — AFTER CP update
    m_dc = std::max(m_dc / 4, static_cast<uint32_t>(1));

    // 5–6. TD computation then update m_latestTimeout
    Time previousTimeout = m_latestTimeout;
    m_latestTimeout = Simulator::Now();
    Time td = m_latestTimeout - previousTimeout;

    // 7–9. ITR update: TI / TD
    Time ti = EstimateTimeoutInterval();

    double rawItr;
    if (td.GetSeconds() > 0.0)
    {
        rawItr = ti.GetSeconds() / td.GetSeconds();
    }
    else
    {
        rawItr = 1.0; // simultaneous/first timeout — congestion signal
    }
    m_itr = m_itrAging * m_itr + (1.0 - m_itrAging) * rawItr;

    // 10. Flag
    m_lastLossWasTimeout = true;

    NS_LOG_INFO("TimeoutAction: TC=" << m_tc << " DC=" << m_dc
                << " CP=" << m_cp << " ITR=" << m_itr
                << " NQD=" << m_nqd
                << " TI=" << ti.GetSeconds() << "s TD=" << td.GetSeconds() << "s");
}

// ═══════════════════════════════════════════════════════════════════════════
// CongestionStateSet — route to DupackAction / TimeoutAction
// ═══════════════════════════════════════════════════════════════════════════

void
TcpNewRenoPlusMod::CongestionStateSet(Ptr<TcpSocketState> tcb,
                                      const TcpSocketState::TcpCongState_t newState)
{
    NS_LOG_FUNCTION(this << tcb << newState);

    if (newState == TcpSocketState::CA_RECOVERY)
    {
        NS_LOG_INFO("Entering CA_RECOVERY (dupack loss)");
        DupackAction();
    }
    else if (newState == TcpSocketState::CA_LOSS)
    {
        NS_LOG_INFO("Entering CA_LOSS (timeout)");
        TimeoutAction();
    }

    TcpNewReno::CongestionStateSet(tcb, newState);
}

// ═══════════════════════════════════════════════════════════════════════════
// GetSsThresh — Three-Factor Decision (the core algorithm)
// ═══════════════════════════════════════════════════════════════════════════

uint32_t
TcpNewRenoPlusMod::GetSsThresh(Ptr<const TcpSocketState> tcb,
                                uint32_t bytesInFlight)
{
    NS_LOG_FUNCTION(this << tcb << bytesInFlight);

    uint32_t cwnd = tcb->m_cWnd.Get();
    uint32_t seg  = tcb->m_segmentSize;

    // Standard NewReno floor values
    uint32_t nrSsthresh = std::max(2 * seg, cwnd / 2);
    uint32_t nrCwnd     = m_lastLossWasTimeout ? seg : (nrSsthresh + 3 * seg);

    uint32_t kSsthresh;
    uint32_t kCwnd;

    // ── Level 1: ALL THREE metrics indicate wireless error (AND) ────────
    if ((m_cp < m_cpThreshold) && (m_nqd < m_nqdThreshold) && (m_itr <= m_itrThreshold))
    {
        kSsthresh = std::max(2 * seg, static_cast<uint32_t>(0.85 * cwnd));
        kCwnd     = std::max(seg,     static_cast<uint32_t>(0.85 * cwnd));
        NS_LOG_INFO("Level 1 WIRELESS (AND): CP=" << m_cp << "<" << m_cpThreshold
                    << " NQD=" << m_nqd << "<" << m_nqdThreshold
                    << " ITR=" << m_itr << "<=" << m_itrThreshold
                    << " -> 0.85*cwnd  kSsthresh=" << kSsthresh
                    << " kCwnd=" << kCwnd);
    }
    // ── Level 2: ANY metric indicates wireless error (OR) ───────────────
    else if ((m_cp < m_cpThreshold) || (m_nqd < m_nqdThreshold) || (m_itr <= m_itrThreshold))
    {
        kSsthresh = std::max(2 * seg, static_cast<uint32_t>(0.75 * cwnd));
        kCwnd     = std::max(seg,     static_cast<uint32_t>(0.75 * cwnd));
        NS_LOG_INFO("Level 2 AMBIGUOUS (OR): CP=" << m_cp
                    << " NQD=" << m_nqd
                    << " ITR=" << m_itr
                    << " -> 0.75*cwnd  kSsthresh=" << kSsthresh
                    << " kCwnd=" << kCwnd);
    }
    // ── Level 3: Congestion (default) ───────────────────────────────────
    else
    {
        kSsthresh = std::max(2 * seg, static_cast<uint32_t>(0.50 * cwnd));
        kCwnd     = std::max(seg,     static_cast<uint32_t>(0.50 * cwnd));
        NS_LOG_INFO("Level 3 CONGESTION: CP=" << m_cp << ">=" << m_cpThreshold
                    << " NQD=" << m_nqd << ">=" << m_nqdThreshold
                    << " ITR=" << m_itr << ">" << m_itrThreshold
                    << " -> 0.50*cwnd  kSsthresh=" << kSsthresh
                    << " kCwnd=" << kCwnd);
    }

    // MAX(K, NR) floor — never be less conservative than standard NewReno
    uint32_t finalSsthresh = std::max(kSsthresh, nrSsthresh);
    uint32_t finalCwnd     = std::max(kCwnd, nrCwnd);

    // For timeout events NS-3 hard-resets cwnd → 1 MSS after this returns.
    // Save our desired cwnd so IncreaseWindow() can restore it on first ACK.
    if (m_lastLossWasTimeout && finalCwnd > seg)
    {
        m_pendingCwnd = finalCwnd;
    }
    else
    {
        m_pendingCwnd = 0;
    }

    NS_LOG_INFO("GetSsThresh: cwnd=" << cwnd << " seg=" << seg
                << " nrSsthresh=" << nrSsthresh << " nrCwnd=" << nrCwnd
                << " finalSsthresh=" << finalSsthresh
                << " finalCwnd=" << finalCwnd
                << " pendingCwnd=" << m_pendingCwnd);

    return finalSsthresh;
}

// ═══════════════════════════════════════════════════════════════════════════
// IncreaseWindow — restore pending cwnd after timeout, then delegate
// ═══════════════════════════════════════════════════════════════════════════

void
TcpNewRenoPlusMod::IncreaseWindow(Ptr<TcpSocketState> tcb, uint32_t segmentsAcked)
{
    NS_LOG_FUNCTION(this << tcb << segmentsAcked);

    if (m_pendingCwnd > 0)
    {
        NS_LOG_INFO("IncreaseWindow: restoring pendingCwnd=" << m_pendingCwnd);
        tcb->m_cWnd = m_pendingCwnd;
        m_pendingCwnd = 0;
    }

    TcpNewReno::IncreaseWindow(tcb, segmentsAcked);
}

} // namespace ns3
