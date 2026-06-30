# IoT-EnergySim

Simulation code for the paper:

> **"A Mechanism for Analyzing the Energy Consumption and Environmental Impact of LoRa Sensors Over Time"**  
> Vandirleya Barbosa, Melissa Alves, Iago Almeida, Iure Fé, Jean Araujo, Francisco Airton Silva  
> *Under review*

This repository provides the NS-3 simulation script used to validate the Stochastic Petri Net (SPN) model proposed in the paper. The simulation models the energy consumption and CO₂ emissions of a LoRa Class A sensor node based on the SX1276 transceiver datasheet parameters.

---

## Requirements

| Component | Version |
|---|---|
| NS-3 | 3.41 |
| LoRaWAN module | v0.3.1 (Signetlab / University of Padova) |
| OS | Ubuntu 22.04 (tested) |
| Compiler | GCC 11+ |

### Installing NS-3.41

```bash
git clone https://gitlab.com/nsnam/ns-3-dev.git
cd ns-3-dev
git checkout ns-3.41
```

### Installing the LoRaWAN module

```bash
cd contrib
git clone https://github.com/signetlabdei/lorawan.git
cd ..
./ns3 configure --enable-examples
./ns3 build
```

---

## Installation

Copy the simulation script to the examples folder of the LoRaWAN module:

```bash
cp lora-energy-sim.cc ns-3-dev/contrib/lorawan/examples/
```

Compile:

```bash
cd ns-3-dev
cmake --build cmake-cache --target lora-energy-sim
```

---

## Usage

```bash
./build/contrib/lorawan/examples/ns3.41-lora-energy-sim-default \
  --SF=7 \
  --PacketInterval=8640 \
  --SimHours=730 \
  --TxCurrentMA=29 \
  --OutputFile=results.csv
```

---

## Output

The simulation generates a CSV file with the following columns:

| Column | Description |
|---|---|
| `time_s` | Simulation time in seconds |
| `battery_pct` | Remaining battery level (%) |
| `packets_sent` | Number of packets transmitted |
| `energy_consumed_J` | Accumulated energy consumption (J) |
| `co2_total_kg` | Accumulated CO₂ emissions (kg) |
| `co2_per_packet_kg` | Average CO₂ emission per packet (kg/pkt) |

---

## License

This project is licensed under the MIT License. See [LICENSE](LICENSE) for details.

---

## Citation

If you use this code in your research, please cite:

```
Barbosa, V., Alves, M., Almeida, I., Fé, I., Araujo, J., Silva, F. A.
"A Mechanism for Analyzing the Energy Consumption and Environmental Impact 
of LoRa Sensors Over Time." Under review.
```
