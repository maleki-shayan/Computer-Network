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
const double PACKET_INTERVAL = 0.3;
const uint32_t MAX_PACKETS = 1000;
const int LARGE_PACKET_SIZE = 1024;
const int SMALL_PACKET_SIZE = 512;
// Changed SSID_NAME to reflect Wi-Fi 6
const std::string SSID_NAME = "wifi6-network";
const std::string IP_BASE = "192.168.1.0";
const std::string IP_MASK = "255.255.255.0";

// std::pair<NetDeviceContainer, NetDeviceContainer>
// SetupWifiNetwork(NodeContainer &stationNodes, NodeContainer &APNode)
// {
//     // Physical layer init - Wi-Fi 6 (802.11ax) // Changed comment to Wi-Fi 6
//     YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
//     YansWifiPhyHelper physicalLayer;
//     physicalLayer.SetChannel(channel.Create());

//     // MAC layer init - Wi-Fi 6 (802.11ax) // Changed comment to Wi-Fi 6
//     WifiHelper wifi;
//     // Changed standard from WIFI_STANDARD_80211ac to WIFI_STANDARD_80211ax
//     wifi.SetStandard(WIFI_STANDARD_80211ax);
    
//     // Added RemoteStationManager to enforce specific MCS for Data and Control modes in Wi-Fi 6
//     wifi.SetRemoteStationManager("ns3::ConstantRateWifiManager", 
//                                  "DataMode", StringValue("HeMcs11"), 
//                                  "ControlMode", StringValue("HeMcs0"));

//     // Configure AP
//     WifiMacHelper mac;
//     Ssid ssid = Ssid(SSID_NAME);
//     mac.SetType("ns3::ApWifiMac", "Ssid", SsidValue(ssid));
    
//     // Added MultiUserScheduler to enable OFDMA and MU-MIMO features for the AP
//     mac.SetMultiUserScheduler("ns3::RrMultiUserScheduler", 
//                               "EnableUlOfdma", BooleanValue(true), 
//                               "EnableBsrp", BooleanValue(true));
                              
//     NetDeviceContainer apDevice = wifi.Install(physicalLayer, mac, APNode);

//     // Configure Stations
//     // Created a separate staMac to avoid applying AP's MultiUserScheduler to stations
//     WifiMacHelper staMac; 
//     staMac.SetType("ns3::StaWifiMac", "Ssid", SsidValue(ssid));
    
//     // Installed using staMac instead of mac
//     NetDeviceContainer stationDevices = wifi.Install(physicalLayer, staMac, stationNodes);

//     // Changed output string to Wi-Fi 6 (802.11ax)
//     std::cout << "Installed Wi-Fi 6 (802.11ax) on all devices" << std::endl;

//     return std::make_pair(apDevice, stationDevices);
// }


std::pair<NetDeviceContainer, NetDeviceContainer>
SetupWifiNetwork(NodeContainer &stationNodes, NodeContainer &APNode)
{
    // --- START OF CHANGES ---

    // Step 1: Create a Spectrum Channel
    // This is the more advanced channel model required for OFDMA
    Ptr<MultiModelSpectrumChannel> spectrumChannel = CreateObject<MultiModelSpectrumChannel>();
    Ptr<FriisPropagationLossModel> lossModel = CreateObject<FriisPropagationLossModel>();
    // Set the frequency for the loss model, e.g., 5.180 GHz for channel 36
    lossModel->SetFrequency(5.180e9);
    Ptr<ConstantSpeedPropagationDelayModel> delayModel = CreateObject<ConstantSpeedPropagationDelayModel>();
    
    spectrumChannel->AddPropagationLossModel(lossModel);
    spectrumChannel->SetPropagationDelayModel(delayModel);

    // Step 2: Create a Spectrum PHY Helper
    // This replaces YansWifiPhyHelper
    SpectrumWifiPhyHelper phy;
    phy.SetChannel(spectrumChannel);
    // Configure Wi-Fi 6 specific settings
    phy.Set("ChannelWidth", UintegerValue(80)); // Set channel width e.g., 20, 40, 80, 160 MHz
    phy.Set("Antennas", UintegerValue(4)); // Example: 4 antennas on devices
    phy.Set("MaxAmpduSize", UintegerValue(0)); // Unlimited AMPDU size

    // --- END OF CHANGES ---

    // MAC layer init - Wi-Fi 6 (802.11ax)
    WifiHelper wifi;
    wifi.SetStandard(WIFI_STANDARD_80211ax);
    
    // Using a simple rate control for this example
    wifi.SetRemoteStationManager("ns3::ConstantRateWifiManager", 
                                 "DataMode", StringValue("HeMcs7"), // A reasonable MCS value
                                 "ControlMode", StringValue("HeMcs0"));

    // Configure AP
    WifiMacHelper mac;
    Ssid ssid = Ssid(SSID_NAME);
    mac.SetType("ns3::ApWifiMac", "Ssid", SsidValue(ssid));
    
    // This part is correct and crucial: enable UL OFDMA in the scheduler
    mac.SetMultiUserScheduler("ns3::RrMultiUserScheduler", 
                              "EnableUlOfdma", BooleanValue(true)); 
                              // "EnableBsrp", BooleanValue(true)); // BSRP is optional but good to have
                              
    // We install using the new phy helper
    NetDeviceContainer apDevice = wifi.Install(phy, mac, APNode);

    // Configure Stations
    WifiMacHelper staMac; 
    staMac.SetType("ns3::StaWifiMac", "Ssid", SsidValue(ssid));
    
    // We install using the new phy helper here as well
    NetDeviceContainer stationDevices = wifi.Install(phy, staMac, stationNodes);

    std::cout << "Installed Wi-Fi 6 (802.11ax) on all devices using SpectrumWifiPhy" << std::endl;

    return std::make_pair(apDevice, stationDevices);
}
void SetupMobility(NodeContainer &APNode, NodeContainer &stationNodes)
{
    MobilityHelper mobilityHelper;
    mobilityHelper.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobilityHelper.Install(APNode);
    mobilityHelper.Install(stationNodes);

    // Fix AP position at (0,0,0)
    Ptr<MobilityModel> apPosition = APNode.Get(0)->GetObject<MobilityModel>();
    apPosition->SetPosition(Vector(0.0, 0.0, 0.0));
    std::cout << "AP position: (0.0, 0.0, 0.0)" << std::endl;

    // Place stations in a circle around AP
    for (int i = 0; i < NUMBER_OF_STATIONS; i++)
    {
        double angleInRadians = i * (2.0 * M_PI) / NUMBER_OF_STATIONS;
        double xPosition = DISTANCE_FROM_AP * cos(angleInRadians);
        double yPosition = DISTANCE_FROM_AP * sin(angleInRadians);

        Ptr<MobilityModel> stationPosition = stationNodes.Get(i)->GetObject<MobilityModel>();
        stationPosition->SetPosition(Vector(xPosition, yPosition, 0.0));

        std::cout << "Station " << i << " placed at (" << xPosition << ", " << yPosition << ")"
                  << std::endl;
    }
}

// Install internet stack
std::pair<Ipv4InterfaceContainer, Ipv4InterfaceContainer>
SetupInternet(NodeContainer &APNode,
              NodeContainer &stationNodes,
              NetDeviceContainer &apDevice,
              NetDeviceContainer &stationDevices)
{
    InternetStackHelper stack;
    stack.Install(APNode);
    stack.Install(stationNodes);

    Ipv4AddressHelper address;
    address.SetBase(IP_BASE.c_str(), IP_MASK.c_str());

    Ipv4InterfaceContainer apInterface = address.Assign(apDevice);
    Ipv4InterfaceContainer staInterfaces = address.Assign(stationDevices);

    std::cout << "AP IP address: " << apInterface.GetAddress(0) << std::endl;
    for (int i = 0; i < NUMBER_OF_STATIONS; i++)
    {
        std::cout << "Station " << i << " IP: " << staInterfaces.GetAddress(i) << std::endl;
    }

    return std::make_pair(apInterface, staInterfaces);
}

void SetupApplications(NodeContainer &APNode,
                       NodeContainer &stationNodes,
                       Ipv4InterfaceContainer &apInterface)
{
    // Echo server on AP
    UdpEchoServerHelper server(PORT_NUMBER);
    ApplicationContainer serverApp = server.Install(APNode.Get(0));
    serverApp.Start(Seconds(0.0));
    serverApp.Stop(Seconds(SIMULATION_TIME));

    std::cout << "Echo server installed on AP, listening on port " << PORT_NUMBER << std::endl;

    // Echo clients on stations
    for (int i = 0; i < NUMBER_OF_STATIONS; i++)
    {
        UdpEchoClientHelper client(apInterface.GetAddress(0), PORT_NUMBER);

        // Alternate packet sizes for different stations
        int packetSize = (i % 2 == 0) ? LARGE_PACKET_SIZE : SMALL_PACKET_SIZE;

        client.SetAttribute("PacketSize", UintegerValue(packetSize));
        client.SetAttribute("Interval", TimeValue(Seconds(PACKET_INTERVAL)));
        client.SetAttribute("MaxPackets", UintegerValue(MAX_PACKETS));

        ApplicationContainer clientApp = client.Install(stationNodes.Get(i));
        clientApp.Start(Seconds(0.0));
        clientApp.Stop(Seconds(SIMULATION_TIME));

        std::cout << "Station " << i << ": sending " << packetSize << " byte packets" << std::endl;
    }
}

void DisplayFlowStatistics(Ptr<FlowMonitor> flowMonitor, FlowMonitorHelper &flowmonHelper,
                           Ipv4InterfaceContainer &apInterface)
{
    flowMonitor->CheckForLostPackets();

    Ptr<Ipv4FlowClassifier> classifier =
        DynamicCast<Ipv4FlowClassifier>(flowmonHelper.GetClassifier());

    std::map<FlowId, FlowMonitor::FlowStats> stats = flowMonitor->GetFlowStats();

    double totalThroughput = 0.0;
    double totalDelay = 0.0;
    int numFlows = 0;
    std::vector<double> throughputValues;

    std::cout << "\n-------------------------------------------" << std::endl;
    // Changed console output header to Wi-Fi 6
    std::cout << "RESULTS - Wi-Fi 6 (802.11ax)" << std::endl;
    std::cout << "-------------------------------------------\n"
              << std::endl;

    for (auto &flow : stats)
    {
        Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow(flow.first);

        if (t.sourceAddress == apInterface.GetAddress(0))
        {
            continue; // This is AP sending back an echo reply → skip it
        }

        double duration =
            flow.second.timeLastRxPacket.GetSeconds() - flow.second.timeFirstTxPacket.GetSeconds();

        double throughput = 0.0;
        if (duration > 0 && flow.second.rxPackets > 0)
        {
            throughput = flow.second.rxBytes * 8.0 / duration / 1000.0;
        }

        double avgDelay = 0.0;
        if (flow.second.rxPackets > 0)
        {
            avgDelay = flow.second.delaySum.GetSeconds() / flow.second.rxPackets;
        }

        double lostPackets = flow.second.lostPackets;
        double txPackets = flow.second.txPackets;
        double lossPercent = 0.0;
        if (txPackets > 0)
        {
            lossPercent = lostPackets / txPackets * 100.0;
        }

        std::cout << "Flow: " << t.sourceAddress << " -> " << t.destinationAddress << std::endl;
        std::cout << "  Sent: " << txPackets << " packets" << std::endl;
        std::cout << "  Received: " << flow.second.rxPackets << " packets" << std::endl;
        std::cout << "  Lost: " << lostPackets << " (" << lossPercent << "%)" << std::endl;
        std::cout << "  Throughput: " << throughput << " kbps" << std::endl;
        std::cout << "  Avg Delay: " << avgDelay * 1000.0 << " ms" << std::endl;
        std::cout << std::endl;

        throughputValues.push_back(throughput);
        totalThroughput += throughput;
        totalDelay += avgDelay;
        numFlows++;
    }

    // Calculate averages and fairness
    double avgThroughput = totalThroughput / numFlows;
    double avgDelayTotal = totalDelay / numFlows;

    // Jain's Fairness Index
    double sum = 0.0;
    double sumSq = 0.0;
    for (double t : throughputValues)
    {
        sum += t;
        sumSq += t * t;
    }
    double jainIndex = (sum * sum) / (numFlows * sumSq);

    std::cout << "-------------------------------------------" << std::endl;
    std::cout << "SUMMARY" << std::endl;
    std::cout << "-------------------------------------------" << std::endl;
    std::cout << "Number of upload flows: " << numFlows << std::endl;
    std::cout << "Average Throughput: " << avgThroughput << " kbps" << std::endl;
    std::cout << "Average Delay: " << avgDelayTotal * 1000.0 << " ms" << std::endl;
    std::cout << "Jain's Fairness Index: " << jainIndex << std::endl;
}

int main(int argc, char *argv[])
{
    LogComponentEnable("UdpEchoClientApplication", LOG_LEVEL_INFO);
    LogComponentEnable("UdpEchoServerApplication", LOG_LEVEL_INFO);

    // Create Nodes
    NodeContainer stationNodes;
    stationNodes.Create(NUMBER_OF_STATIONS);
    NodeContainer APNode;
    APNode.Create(1);

    std::cout << "Number of Stations: " << stationNodes.GetN() << " | Number of APs: " << APNode.GetN() << std::endl;

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
