#include "tcp-newreno-plus.h"
#include "ns3/log.h"
#include "ns3/simulator.h"

namespace ns3
{

NS_LOG_COMPONENT_DEFINE("TcpNewRenoPlus");
NS_OBJECT_ENSURE_REGISTERED(TcpNewRenoPlus);

TypeId
TcpNewRenoPlus::GetTypeId()
{
    static TypeId tid = TypeId("ns3::TcpNewRenoPlus")
                            .SetParent<TcpNewReno>()
                            .SetGroupName("Internet")
                            .AddConstructor<TcpNewRenoPlus>()
                            .AddAttribute("TdrThreshold",
                                          "TDR threshold (TDR_T) for bit error detection",
                                          DoubleValue(0.10),
                                          MakeDoubleAccessor(&TcpNewRenoPlus::m_tdrThreshold),
                                          MakeDoubleChecker<double>(0.0, 1.0))
                            .AddAttribute("TdrAgingFactor",
                                          "TDR aging factor (TDR_A) for EWMA",
                                          DoubleValue(0.875),
                                          MakeDoubleAccessor(&TcpNewRenoPlus::m_tdrAgingFactor),
                                          MakeDoubleChecker<double>(0.0, 1.0))
                            .AddAttribute("ItrThreshold",
                                          "ITR threshold (ITR_T) for bit error detection",
                                          DoubleValue(0.25),
                                          MakeDoubleAccessor(&TcpNewRenoPlus::m_itrThreshold),
                                          MakeDoubleChecker<double>(0.0, 1.0))
                            .AddAttribute("ItrAgingFactor",
                                          "ITR aging factor (ITR_A) for EWMA",
                                          DoubleValue(0.875),
                                          MakeDoubleAccessor(&TcpNewRenoPlus::m_itrAgingFactor),
                                          MakeDoubleChecker<double>(0.0, 1.0));
    return tid;
}

TcpNewRenoPlus::TcpNewRenoPlus()
    : TcpNewReno(),
      m_tc(0),
      m_dc(0),
      m_tdr(0.0),
      m_tdrThreshold(0.10),
      m_tdrAgingFactor(0.875),
      m_itr(0.0),
      m_itrThreshold(0.25),
      m_itrAgingFactor(0.875),
      m_latestTimeout(Seconds(0)),
      m_rttEstimate(Seconds(0)),
      m_pendingCwnd(0),
      m_lastLossWasTimeout(false)
{
    NS_LOG_FUNCTION(this);
}

TcpNewRenoPlus::TcpNewRenoPlus(const TcpNewRenoPlus& sock)
    : TcpNewReno(sock),
      m_tc(sock.m_tc),
      m_dc(sock.m_dc),
      m_tdr(sock.m_tdr),
      m_tdrThreshold(sock.m_tdrThreshold),
      m_tdrAgingFactor(sock.m_tdrAgingFactor),
      m_itr(sock.m_itr),
      m_itrThreshold(sock.m_itrThreshold),
      m_itrAgingFactor(sock.m_itrAgingFactor),
      m_latestTimeout(sock.m_latestTimeout),
      m_rttEstimate(sock.m_rttEstimate),
      m_pendingCwnd(sock.m_pendingCwnd),
      m_lastLossWasTimeout(sock.m_lastLossWasTimeout)
{
    NS_LOG_FUNCTION(this);
}

TcpNewRenoPlus::~TcpNewRenoPlus()
{
    NS_LOG_FUNCTION(this);
}

std::string
TcpNewRenoPlus::GetName() const
{
    return "TcpNewRenoPlus";
}

Ptr<TcpCongestionOps>
TcpNewRenoPlus::Fork()
{
    return CopyObject<TcpNewRenoPlus>(this);
}

void
TcpNewRenoPlus::PktsAcked(Ptr<TcpSocketState> tcb, uint32_t segmentsAcked, const Time& rtt)
{
    NS_LOG_FUNCTION(this << tcb << segmentsAcked << rtt);

    if (rtt.IsZero())
    {
        return;
    }

    if (m_rttEstimate.IsZero())
    {
        // First RTT sample: initialise directly
        m_rttEstimate = rtt;
    }
    else
    {
        // EWMA update: alpha = 0.875 (weight on old estimate), 0.125 on new sample
        m_rttEstimate = m_rttEstimate * 0.875 + rtt * 0.125;
    }

    NS_LOG_INFO("PktsAcked: rtt=" << rtt.GetSeconds() << "s"
                << " rttEstimate=" << m_rttEstimate.GetSeconds() << "s");
}

void
TcpNewRenoPlus::IncreaseWindow(Ptr<TcpSocketState> tcb, uint32_t segmentsAcked)
{
    NS_LOG_FUNCTION(this << tcb << segmentsAcked);

    if (m_pendingCwnd > 0)
    {
        NS_LOG_INFO("IncreaseWindow: restoring pending cwnd=" << m_pendingCwnd);
        tcb->m_cWnd = m_pendingCwnd;
        m_pendingCwnd = 0;
    }

    TcpNewReno::IncreaseWindow(tcb, segmentsAcked);
}

Time
TcpNewRenoPlus::EstimateTimeoutInterval() const
{
    if (m_rttEstimate.IsZero())
    {
        return Seconds(1.0);
    }
    return std::max(m_rttEstimate * 3, Seconds(1.0));
}

// Algorithm 1: DupackAction — runs on CA_RECOVERY (dupack-triggered loss)
void
TcpNewRenoPlus::DupackAction()
{
    NS_LOG_FUNCTION(this);

    // Increment the dupack counter
    m_dc++;

    // Update TDR via EWMA (TC / DC ratio)
    double tcDcRatio = (double)m_tc / std::max(m_dc, (uint32_t)1);
    m_tdr = (m_tdrAgingFactor * m_tdr) + ((1.0 - m_tdrAgingFactor) * tcDcRatio);

    // TD = time elapsed since the last actual timeout event
    Time td = Simulator::Now() - m_latestTimeout;

    // TI = current retransmission timeout interval estimate (paper definition)
    Time ti = EstimateTimeoutInterval();

    // Update ITR via EWMA (TI / TD ratio — not capped at 1.0)
    double tiTdRatio;
    if (td.GetSeconds() > 0.0)
    {
        tiTdRatio = ti.GetSeconds() / td.GetSeconds();
    }
    else
    {
        // No prior timeout yet — assume non-congestion (small ratio)
        tiTdRatio = 0.01;
    }
    m_itr = (m_itrAgingFactor * m_itr) + ((1.0 - m_itrAgingFactor) * tiTdRatio);

    // NOTE: m_latestTimeout is NOT updated here; only actual timeouts update it.

    NS_LOG_INFO("DupackAction: TC=" << m_tc << " DC=" << m_dc
                << " TDR=" << m_tdr << " ITR=" << m_itr
                << " TI=" << ti.GetSeconds() << "s TD=" << td.GetSeconds() << "s");
}

// Algorithm 2: TimeoutAction — runs on CA_LOSS (RTO-triggered loss)
void
TcpNewRenoPlus::TimeoutAction()
{
    NS_LOG_FUNCTION(this);

    // Increment the timeout counter
    m_tc++;

    // Update TDR via EWMA — use a high ratio (10.0) when DC is still zero
    // to correctly signal congestion (10.0 >> TDR_T)
    double tcDcRatio = (m_dc > 0) ? ((double)m_tc / (double)m_dc) : 10.0;
    m_tdr = (m_tdrAgingFactor * m_tdr) + ((1.0 - m_tdrAgingFactor) * tcDcRatio);

    // DC decay — must happen AFTER TDR is updated
    m_dc = std::max(m_dc / 4, (uint32_t)1);

    // Compute TD from previous timeout before updating m_latestTimeout
    Time previousTimeout = m_latestTimeout;
    m_latestTimeout = Simulator::Now();
    Time td = m_latestTimeout - previousTimeout;

    // TI = current retransmission timeout interval estimate (paper definition)
    Time ti = EstimateTimeoutInterval();

    // Update ITR via EWMA (TI / TD ratio — not capped at 1.0)
    double tiTdRatio;
    if (td.GetSeconds() > 0.0)
    {
        tiTdRatio = ti.GetSeconds() / td.GetSeconds();
    }
    else
    {
        // Simultaneous or first timeout — assume congestion (high ratio)
        tiTdRatio = 1.0;
    }
    m_itr = (m_itrAgingFactor * m_itr) + ((1.0 - m_itrAgingFactor) * tiTdRatio);

    NS_LOG_INFO("TimeoutAction: TC=" << m_tc << " DC=" << m_dc
                << " TDR=" << m_tdr << " ITR=" << m_itr
                << " TI=" << ti.GetSeconds() << "s TD=" << td.GetSeconds() << "s");
}

// CongestionStateSet — route CA_RECOVERY → DupackAction, CA_LOSS → TimeoutAction
void
TcpNewRenoPlus::CongestionStateSet(Ptr<TcpSocketState> tcb,
                                   const TcpSocketState::TcpCongState_t newState)
{
    NS_LOG_FUNCTION(this << tcb << newState);

    if (newState == TcpSocketState::CA_RECOVERY)
    {
        NS_LOG_INFO("Entering CA_RECOVERY (dupack loss)");
        m_lastLossWasTimeout = false;
        DupackAction();
    }
    else if (newState == TcpSocketState::CA_LOSS)
    {
        NS_LOG_INFO("Entering CA_LOSS (timeout)");
        m_lastLossWasTimeout = true;
        TimeoutAction();
    }

    // Call parent implementation
    TcpNewReno::CongestionStateSet(tcb, newState);
}

// Algorithm 3: GetSsThresh with Level 1 (AND) and Level 2 (OR) logic
uint32_t
TcpNewRenoPlus::GetSsThresh(Ptr<const TcpSocketState> tcb, uint32_t bytesInFlight)
{
    NS_LOG_FUNCTION(this << tcb << bytesInFlight);

    // Paper Algorithm 3: compute NR (standard NewReno) values
    uint32_t cwnd = tcb->m_cWnd.Get();
    uint32_t segSize = tcb->m_segmentSize;
    uint32_t nrSsthresh = std::max(2 * segSize, cwnd / 2);

    // Per the paper:
    //   DUPACK  → NRcwnd = NRssthresh + 3  (fast recovery inflation)
    //   TIMEOUT → NRcwnd = 1 segment
    uint32_t nrCwnd;
    if (!m_lastLossWasTimeout)
    {
        nrCwnd = nrSsthresh + 3 * segSize;
    }
    else
    {
        nrCwnd = segSize;
    }

    // Initialize K values with NewReno defaults
    uint32_t kSsthresh = nrSsthresh;
    uint32_t kCwnd = nrCwnd;

    // Level 1 (AND): both thresholds indicate bit error
    if ((m_tdr < m_tdrThreshold) && (m_itr < m_itrThreshold))
    {
        kSsthresh = std::max(2 * segSize, (3 * cwnd) / 4);
        kCwnd = std::max(segSize, cwnd / 2);
        NS_LOG_INFO("BIT ERROR (AND): TDR=" << m_tdr << " < " << m_tdrThreshold
                    << " && ITR=" << m_itr << " < " << m_itrThreshold
                    << " -> kSsthresh=" << kSsthresh << " kCwnd=" << kCwnd);
    }
    // Level 2 (OR): one threshold indicates mixed condition
    else if ((m_tdr < m_tdrThreshold) || (m_itr < m_itrThreshold))
    {
        kSsthresh = std::max(2 * segSize, (2 * cwnd) / 3);
        kCwnd = nrCwnd; // standard NewReno cwnd
        NS_LOG_INFO("MIXED (OR): TDR=" << m_tdr << " ITR=" << m_itr
                    << " -> kSsthresh=" << kSsthresh << " kCwnd=" << kCwnd);
    }
    else
    {
        // Congestion loss — no thresholds met
        kSsthresh = nrSsthresh;
        kCwnd = nrCwnd;
        NS_LOG_INFO("CONGESTION: TDR=" << m_tdr << " >= " << m_tdrThreshold
                    << " && ITR=" << m_itr << " >= " << m_itrThreshold
                    << " -> kSsthresh=" << kSsthresh << " kCwnd=" << kCwnd);
    }

    // MAX(K, NR) floor guarantees we never go below standard NewReno
    uint32_t finalSsthresh = std::max(kSsthresh, nrSsthresh);
    uint32_t finalCwnd = std::max(kCwnd, nrCwnd);

    // For timeout events NS-3 hard-resets cwnd to 1 MSS after this returns.
    // Save our desired cwnd so IncreaseWindow() can restore it on the first ACK.
    if (m_lastLossWasTimeout && finalCwnd > tcb->m_segmentSize)
    {
        m_pendingCwnd = finalCwnd;
    }
    else
    {
        m_pendingCwnd = 0;
    }

    NS_LOG_INFO("Final values: ssthresh=" << finalSsthresh << " cwnd=" << finalCwnd
                << " pendingCwnd=" << m_pendingCwnd);

    return finalSsthresh;
}

} // namespace ns3
