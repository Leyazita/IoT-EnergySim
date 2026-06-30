#include "ns3/basic-energy-source.h"
#include "ns3/basic-energy-source-helper.h"
#include "ns3/command-line.h"
#include "ns3/constant-position-mobility-model.h"
#include "ns3/double.h"
#include "ns3/end-device-lora-phy.h"
#include "ns3/end-device-lorawan-mac.h"
#include "ns3/energy-module.h"
#include "ns3/energy-source-container.h"
#include "ns3/gateway-lora-phy.h"
#include "ns3/gateway-lorawan-mac.h"
#include "ns3/log.h"
#include "ns3/lora-channel.h"
#include "ns3/lora-helper.h"
#include "ns3/lora-phy-helper.h"
#include "ns3/lora-radio-energy-model-helper.h"
#include "ns3/lora-radio-energy-model.h"
#include "ns3/lorawan-mac-helper.h"
#include "ns3/mobility-helper.h"
#include "ns3/node-container.h"
#include "ns3/periodic-sender-helper.h"
#include "ns3/propagation-delay-model.h"
#include "ns3/propagation-loss-model.h"
#include "ns3/simulator.h"
#include <fstream>
#include <iostream>

using namespace ns3;
using namespace lorawan;

NS_LOG_COMPONENT_DEFINE("LoraEnergySim");

static const double BATTERY_CAPACITY_MAH = 2400.0;
static const double BATTERY_VOLTAGE_V    = 3.0;
static const double EMISSION_FACTOR      = 0.475;

static const double I_IDLE_A  = 1.5e-6;
static const double I_RX_A    = 10.8e-3;
static const double I_SLEEP_A = 0.2e-6;

static uint32_t    SF                = 7;
static double      PACKET_INTERVAL_S = 8640.0;
static double      SIM_DURATION_H    = 730.0;
static double      SAMPLE_INTERVAL_S = 3600.0;
static uint32_t    PAYLOAD_SIZE      = 12;
static std::string OUTPUT_FILE       = "lora-results.csv";
static double      TX_CURRENT_MA     = 29.0;

static uint32_t      g_packetsSent = 0;
static double        g_remainingJ  = 0.0;
static double        g_initialJ    = 0.0;
static std::ofstream g_outFile;

void OnRemainingEnergyChanged(double oldVal, double newVal)
{
    g_remainingJ = newVal;
}

void OnPacketSent(Ptr<Packet const> packet, uint32_t systemId)
{
    g_packetsSent++;
}

void SampleMetrics(double simDurationS)
{
    double now         = Simulator::Now().GetSeconds();
    double batteryPct  = (g_remainingJ / g_initialJ) * 100.0;
    double consumedJ   = g_initialJ - g_remainingJ;
    double consumedKWh = consumedJ / 3.6e6;
    double co2total    = consumedKWh * EMISSION_FACTOR;
    double co2pkt      = (g_packetsSent > 0) ? co2total / g_packetsSent : 0.0;

    g_outFile << now           << ","
              << batteryPct    << ","
              << g_packetsSent << ","
              << consumedJ     << ","
              << co2total      << ","
              << co2pkt        << "\n";
    g_outFile.flush();

    if (now + SAMPLE_INTERVAL_S <= simDurationS)
        Simulator::Schedule(Seconds(SAMPLE_INTERVAL_S), &SampleMetrics, simDurationS);
}

int main(int argc, char* argv[])
{
    CommandLine cmd;
    cmd.AddValue("SF",             "Spreading Factor (7-12)",                    SF);
    cmd.AddValue("PacketInterval", "Interval between packets in seconds",        PACKET_INTERVAL_S);
    cmd.AddValue("SimHours",       "Simulation duration in hours",               SIM_DURATION_H);
    cmd.AddValue("OutputFile",     "Output CSV file path",                       OUTPUT_FILE);
    cmd.AddValue("SampleInterval", "Sampling interval in seconds",               SAMPLE_INTERVAL_S);
    cmd.AddValue("PayloadSize",    "Payload size in bytes",                      PAYLOAD_SIZE);
    cmd.AddValue("TxCurrentMA",    "TX current in mA (20/29/87/120 - SX1276)",  TX_CURRENT_MA);
    cmd.Parse(argc, argv);

    double simDurationS = SIM_DURATION_H * 3600.0;
    double I_TX_A       = TX_CURRENT_MA * 1e-3;

    g_initialJ   = BATTERY_CAPACITY_MAH * 1e-3 * BATTERY_VOLTAGE_V * 3600.0;
    g_remainingJ = g_initialJ;

    g_outFile.open(OUTPUT_FILE);
    g_outFile << "time_s,battery_pct,packets_sent,"
              << "energy_consumed_J,co2_total_kg,co2_per_packet_kg\n";

    std::cout << "SF=" << SF
              << " | Interval=" << PACKET_INTERVAL_S << "s"
              << " | " << 86400.0 / PACKET_INTERVAL_S << " pkt/day"
              << " | TX=" << TX_CURRENT_MA << "mA"
              << " | Sim=" << SIM_DURATION_H << "h"
              << " | Output=" << OUTPUT_FILE << std::endl;

    Ptr<LogDistancePropagationLossModel> loss =
        CreateObject<LogDistancePropagationLossModel>();
    loss->SetPathLossExponent(3.76);
    loss->SetReference(1, 7.7);

    Ptr<PropagationDelayModel> delay =
        CreateObject<ConstantSpeedPropagationDelayModel>();

    Ptr<LoraChannel> channel = CreateObject<LoraChannel>(loss, delay);

    LoraPhyHelper    phyHelper;
    phyHelper.SetChannel(channel);
    LorawanMacHelper macHelper;
    LoraHelper       helper;

    NodeContainer endDevices;
    endDevices.Create(1);

    MobilityHelper mob;
    mob.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mob.Install(endDevices);
    endDevices.Get(0)->GetObject<MobilityModel>()->SetPosition(Vector(0, 0, 0));

    phyHelper.SetDeviceType(LoraPhyHelper::ED);
    macHelper.SetDeviceType(LorawanMacHelper::ED_A);
    NetDeviceContainer edDevices = helper.Install(phyHelper, macHelper, endDevices);

    Ptr<EndDeviceLorawanMac> mac =
        endDevices.Get(0)->GetDevice(0)
            ->GetObject<LoraNetDevice>()->GetMac()
            ->GetObject<EndDeviceLorawanMac>();
    mac->SetDataRate(12 - SF);

    NodeContainer gateways;
    gateways.Create(1);

    MobilityHelper mobGW;
    mobGW.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobGW.Install(gateways);
    gateways.Get(0)->GetObject<MobilityModel>()->SetPosition(Vector(0, 0, 15));

    phyHelper.SetDeviceType(LoraPhyHelper::GW);
    macHelper.SetDeviceType(LorawanMacHelper::GW);
    helper.Install(phyHelper, macHelper, gateways);

    BasicEnergySourceHelper energyHelper;
    energyHelper.Set("BasicEnergySourceInitialEnergyJ", DoubleValue(g_initialJ));
    energyHelper.Set("BasicEnergySupplyVoltageV",       DoubleValue(BATTERY_VOLTAGE_V));
    EnergySourceContainer sources = energyHelper.Install(endDevices);

    Ptr<BasicEnergySource> src =
        DynamicCast<BasicEnergySource>(sources.Get(0));
    src->TraceConnectWithoutContext("RemainingEnergy",
        MakeCallback(&OnRemainingEnergyChanged));

    LoraRadioEnergyModelHelper radioHelper;
    radioHelper.Set("StandbyCurrentA", DoubleValue(I_IDLE_A));
    radioHelper.Set("TxCurrentA",      DoubleValue(I_TX_A));
    radioHelper.Set("RxCurrentA",      DoubleValue(I_RX_A));
    radioHelper.Set("SleepCurrentA",   DoubleValue(I_SLEEP_A));
    radioHelper.SetTxCurrentModel("ns3::ConstantLoraTxCurrentModel",
                                  "TxCurrent", DoubleValue(I_TX_A));
    radioHelper.Install(edDevices, sources);

    PeriodicSenderHelper senderHelper;
    senderHelper.SetPeriod(Seconds(PACKET_INTERVAL_S));
    senderHelper.SetPacketSize(PAYLOAD_SIZE);
    ApplicationContainer apps = senderHelper.Install(endDevices);
    apps.Start(Seconds(1));
    apps.Stop(Seconds(simDurationS));

    Ptr<EndDeviceLoraPhy> phy =
        endDevices.Get(0)->GetDevice(0)
            ->GetObject<LoraNetDevice>()->GetPhy()
            ->GetObject<EndDeviceLoraPhy>();
    phy->TraceConnectWithoutContext("StartSending",
        MakeCallback(&OnPacketSent));

    Simulator::Schedule(Seconds(SAMPLE_INTERVAL_S), &SampleMetrics, simDurationS);
    Simulator::Stop(Seconds(simDurationS));
    Simulator::Run();
    Simulator::Destroy();

    double consumedJ   = g_initialJ - g_remainingJ;
    double consumedKWh = consumedJ / 3.6e6;
    double co2         = consumedKWh * EMISSION_FACTOR;

    std::cout << "\n=== Final Results ===" << std::endl;
    std::cout << "Packets sent:      " << g_packetsSent << std::endl;
    std::cout << "Battery remaining: " << (g_remainingJ / g_initialJ) * 100.0 << "%" << std::endl;
    std::cout << "Energy consumed:   " << consumedJ << " J" << std::endl;
    std::cout << "CO2 total:         " << co2 << " kg" << std::endl;
    if (g_packetsSent > 0)
        std::cout << "CO2 per packet:    " << co2 / g_packetsSent << " kg/pkt" << std::endl;

    g_outFile.close();
    return 0;
}
