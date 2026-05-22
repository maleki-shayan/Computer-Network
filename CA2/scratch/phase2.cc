#include "ns3/applications-module.h"
#include "ns3/core-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/internet-module.h"
#include "ns3/mobility-module.h"
#include "ns3/network-module.h"
#include "ns3/wifi-module.h"
#include "ns3/spectrum-module.h"

#include <cmath>

using namespace ns3;

const int NUMBER_OF_STATIONS = 5;
const double DISTANCE_FROM_AP = 5.0;
const uint16_t PORT_NUMBER = 9;
const double SIMULATION_TIME = 10.0;
const double PACKET_INTERVAL = 0.5;        
const uint32_t MAX_PACKETS = 20;           
const int LARGE_PACKET_SIZE = 1024;        // 1st, 3rd, 5th STA
const int SMALL_PACKET_SIZE = 512;         // 2nd, 4th STA
const int TRANMISSION_ANTENNA = 4;
const int REACIEVER_ANTENNA = 4;
const std::string SSID_NAME = "wifi6-network";
const std::string IP_BASE = "192.168.1.0";
const std::string IP_MASK = "255.255.255.0";

std::pair<NetDeviceContainer, NetDeviceContainer>
SetupWifiNetwork(NodeContainer &stationNodes, NodeContainer &APNode)
{
    // we changed the physical layer to spectrom so it can handle multiple bandwidths
    SpectrumWifiPhyHelper phy;
    SpectrumChannelHelper channelHelper = SpectrumChannelHelper::Default();
    channelHelper.SetChannel("ns3::MultiModelSpectrumChannel");
    phy.SetChannel(channelHelper.Create());

    //NOTE : these lines implement the MU_MIMO antennas
    phy.Set("Antennas", UintegerValue(4));
    phy.Set("MaxSupportedTxSpatialStreams", UintegerValue(TRANMISSION_ANTENNA));
    phy.Set("MaxSupportedRxSpatialStreams", UintegerValue(REACIEVER_ANTENNA));

    // increased the channel bandwidth to 40MHz so that wouldnt crash
    phy.Set("ChannelSettings", StringValue("{38, 40, BAND_5GHZ, 0}"));

    WifiHelper wifi;
    wifi.SetStandard(WIFI_STANDARD_80211ax);

    wifi.SetRemoteStationManager("ns3::ConstantRateWifiManager",
                                 "DataMode", StringValue("HeMcs7"), // we set the highest speed for data mod to send the packets as fast as possible
                                 "ControlMode", StringValue("HeMcs5")); // we set it to HeMCs5 because we need more reliablity for conltrol signals 

    WifiMacHelper mac;
    Ssid ssid = Ssid(SSID_NAME);

    // AP with OFDMA scheduler
    mac.SetType("ns3::ApWifiMac", "Ssid", SsidValue(ssid));
    mac.SetMultiUserScheduler("ns3::RrMultiUserScheduler",
                              "EnableUlOfdma", BooleanValue(true),
                              "EnableBsrp", BooleanValue(true),
                              "NStations", UintegerValue(8),       // up to 8 per MU frame
                            //   "UlPsduSize", UintegerValue(256), // this line was commented to avoid fragmentation of 1024
                              "UseCentral26TonesRus", BooleanValue(false));

    NetDeviceContainer apDevice = wifi.Install(phy, mac, APNode);

    mac.SetType("ns3::StaWifiMac", "Ssid", SsidValue(ssid),
                "ActiveProbing", BooleanValue(false));
    NetDeviceContainer stationDevices = wifi.Install(phy, mac, stationNodes);

    std::cout << "Installed Wi-Fi 6 (802.11ax) – 40 MHz channel" << std::endl;

    return std::make_pair(apDevice, stationDevices);
}


void SetupMobility(NodeContainer &APNode, NodeContainer &stationNodes)
{
    MobilityHelper mobilityHelper;
    mobilityHelper.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobilityHelper.Install(APNode);
    mobilityHelper.Install(stationNodes);

    Ptr<MobilityModel> apPosition = APNode.Get(0)->GetObject<MobilityModel>();
    apPosition->SetPosition(Vector(0.0, 0.0, 0.0));

    for (int i = 0; i < NUMBER_OF_STATIONS; i++)
    {
        double angle = i * (2.0 * M_PI) / NUMBER_OF_STATIONS;
        double x = DISTANCE_FROM_AP * cos(angle);
        double y = DISTANCE_FROM_AP * sin(angle);
        Ptr<MobilityModel> staPos = stationNodes.Get(i)->GetObject<MobilityModel>();
        staPos->SetPosition(Vector(x, y, 0.0));
    }
    std::cout << NUMBER_OF_STATIONS << " stations in a " << DISTANCE_FROM_AP
              << "m circle around AP" << std::endl;
}

std::pair<Ipv4InterfaceContainer, Ipv4InterfaceContainer>
SetupInternet(NodeContainer &APNode, NodeContainer &stationNodes,
              NetDeviceContainer &apDevice, NetDeviceContainer &stationDevices)
{
    InternetStackHelper stack;
    stack.Install(APNode);
    stack.Install(stationNodes);

    Ipv4AddressHelper address;
    address.SetBase(IP_BASE.c_str(), IP_MASK.c_str());

    Ipv4InterfaceContainer apInterface = address.Assign(apDevice);
    Ipv4InterfaceContainer staInterfaces = address.Assign(stationDevices);

    std::cout << "AP IP: " << apInterface.GetAddress(0) << std::endl;
    std::cout << "STA IP range: " << staInterfaces.GetAddress(0)
              << " – " << staInterfaces.GetAddress(NUMBER_OF_STATIONS - 1) << std::endl;

    return std::make_pair(apInterface, staInterfaces);
}

void SetupApplications(NodeContainer &APNode, NodeContainer &stationNodes,
                       Ipv4InterfaceContainer &apInterface)
{
    UdpEchoServerHelper server(PORT_NUMBER);
    ApplicationContainer serverApp = server.Install(APNode.Get(0));
    serverApp.Start(Seconds(0.0));
    serverApp.Stop(Seconds(SIMULATION_TIME));

    for (int i = 0; i < NUMBER_OF_STATIONS; i++)
    {
        UdpEchoClientHelper client(apInterface.GetAddress(0), PORT_NUMBER);
        int packetSize = (i % 2 == 0) ? LARGE_PACKET_SIZE : SMALL_PACKET_SIZE;

        client.SetAttribute("PacketSize", UintegerValue(packetSize));
        client.SetAttribute("Interval", TimeValue(Seconds(PACKET_INTERVAL)));
        client.SetAttribute("MaxPackets", UintegerValue(MAX_PACKETS));

        ApplicationContainer clientApp = client.Install(stationNodes.Get(i));
        clientApp.Start(Seconds(0.0));
        clientApp.Stop(Seconds(SIMULATION_TIME));
    }
    std::cout << "Traffic: " << NUMBER_OF_STATIONS << " clients → AP, "
              << PACKET_INTERVAL * 1000 << "ms interval, "
              << LARGE_PACKET_SIZE << "/" << SMALL_PACKET_SIZE << "B packets" << std::endl;
}

void DisplayFlowStatistics(Ptr<FlowMonitor> flowMonitor, FlowMonitorHelper &flowmonHelper,
                           Ipv4InterfaceContainer &apInterface)
{
    flowMonitor->CheckForLostPackets();
    Ptr<Ipv4FlowClassifier> classifier =
        DynamicCast<Ipv4FlowClassifier>(flowmonHelper.GetClassifier());
    std::map<FlowId, FlowMonitor::FlowStats> stats = flowMonitor->GetFlowStats();

    double totalThroughput = 0.0, totalDelay = 0.0;
    double totalTx = 0.0, totalRx = 0.0, totalLost = 0.0;
    int numFlows = 0;
    std::vector<double> throughputValues;

    std::cout << "\n═══════════════════════════════════════════════" << std::endl;
    std::cout << "RESULTS – Wi‑Fi 6 (802.11ax) OFDMA + MU‑MIMO" << std::endl;
    std::cout << "═══════════════════════════════════════════════\n" << std::endl;

    for (auto &flow : stats)
    {
        Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow(flow.first);
        if (t.sourceAddress == apInterface.GetAddress(0))
            continue;
        double txPackets = flow.second.txPackets;
        double rxPackets = flow.second.rxPackets;
        double trueLostPackets = txPackets - rxPackets;
        
        double duration = flow.second.timeLastRxPacket.GetSeconds() -
                          flow.second.timeFirstTxPacket.GetSeconds();
        double throughput = (duration > 0 && rxPackets > 0)
            ? flow.second.rxBytes * 8.0 / duration / 1000.0 : 0.0;
        double avgDelay = (rxPackets > 0)
            ? flow.second.delaySum.GetSeconds() / rxPackets : 0.0;
        double lossPercent = (txPackets > 0)
            ? (trueLostPackets / txPackets) * 100.0 : 0.0;

        std::cout << "Flow " << numFlows + 1 << ": "
                  << t.sourceAddress << " → " << t.destinationAddress << std::endl;
        std::cout << "  Tx: " << txPackets
                  << "  Rx: " << rxPackets
                  << "  Lost: " << trueLostPackets
                  << " (" << lossPercent << "%)" << std::endl;
        std::cout << "  Throughput: " << throughput << " kbps"
                  << "  Delay: " << avgDelay * 1000.0 << " ms\n" << std::endl;

        
        totalLost += trueLostPackets;
        throughputValues.push_back(throughput);
        totalThroughput += throughput;
        totalDelay += avgDelay;
        totalTx += flow.second.txPackets;
        totalRx += flow.second.rxPackets;
        numFlows++;
    }

    double avgThroughput = totalThroughput / numFlows;
    double avgDelayTotal = totalDelay / numFlows;

    double sum = 0.0, sumSq = 0.0;
    for (double t : throughputValues)
    {
        sum += t;
        sumSq += t * t;
    }
    double jainIndex = (numFlows * sumSq) > 0 ? (sum * sum) / (numFlows * sumSq) : 0.0;

    std::cout << "═══════════════════════════════════════════════" << std::endl;
    std::cout << "SUMMARY" << std::endl;
    std::cout << "═══════════════════════════════════════════════" << std::endl;
    std::cout << "Flows: " << numFlows << std::endl;
    std::cout << "Total Tx: " << totalTx << "  Rx: " << totalRx
              << "  Lost: " << totalLost
              << " (" << (totalTx > 0 ? totalLost / totalTx * 100.0 : 0.0) << "%)" << std::endl;
    std::cout << "Average Throughput: " << avgThroughput << " kbps" << std::endl;
    std::cout << "Total Throughput: " << totalThroughput << " kbps" << std::endl;
    std::cout << "Average Delay: " << avgDelayTotal * 1000.0 << " ms" << std::endl;
    std::cout << "Jain's Fairness Index: " << jainIndex << std::endl;
}

int main(int argc, char *argv[])
{
    // LogComponentEnable("UdpEchoClientApplication", LOG_LEVEL_INFO);
    // LogComponentEnable("UdpEchoServerApplication", LOG_LEVEL_INFO);

    NodeContainer stationNodes;
    stationNodes.Create(NUMBER_OF_STATIONS);
    NodeContainer APNode;
    APNode.Create(1);

    std::cout << "\n╔══════════════════════════════════════════╗" << std::endl;
    std::cout << "║   Wi‑Fi 6 (802.11ax) – Phase 2           ║" << std::endl;
    std::cout << "║   OFDMA + MU‑MIMO                        ║" << std::endl;
    std::cout << "╚══════════════════════════════════════════╝\n" << std::endl;
    std::cout << "Stations: " << stationNodes.GetN() << " | AP: " << APNode.GetN() << std::endl;

    auto [apDevice, stationDevices] = SetupWifiNetwork(stationNodes, APNode);
    SetupMobility(APNode, stationNodes);
    auto [apInterface, staInterfaces] =
        SetupInternet(APNode, stationNodes, apDevice, stationDevices);
    SetupApplications(APNode, stationNodes, apInterface);

    FlowMonitorHelper flowmonHelper;
    Ptr<FlowMonitor> flowMonitor = flowmonHelper.InstallAll();

    std::cout << "\nRunning simulation for " << SIMULATION_TIME << " seconds..." << std::endl;

    Simulator::Stop(Seconds(SIMULATION_TIME));
    Simulator::Run();

    DisplayFlowStatistics(flowMonitor, flowmonHelper, apInterface);
    Simulator::Destroy();

    return 0;
}