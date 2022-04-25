INPUT_FILE = "loglistener.txt"

delays = {}
pkt_sent = {}
pkt_received = {}


def main():
    with open(INPUT_FILE, "r") as f:
        for line in f:
            if not (("INFO: App" in line) and ("Sent_to" in line or "Received_from" in line)):
                continue
            if "Sent_to" in line:
                fields = line.split()
                node_id = int(fields[1][3:])
                if node_id in pkt_sent:
                    pkt_sent[node_id][int(fields[-1])] = float(fields[0])
                else:
                    pkt_sent[node_id] = {}
                    pkt_sent[node_id][int(fields[-1])] = float(fields[0])
            elif "Received_from" in line:
                fields = line.split()
                node_id = int(fields[6])
                if node_id in pkt_received:
                    pkt_received[node_id][int(fields[-1])] = float(fields[0])
                else:
                    pkt_received[node_id] = {}
                    pkt_received[node_id][int(fields[-1])] = float(fields[0])
            else:
                print("Error: line not recognized")

    for node_id in pkt_sent.keys():
        if node_id in pkt_received:
            pkt_id_list = sorted(set(pkt_sent[node_id].keys()) & set(
                pkt_received[node_id].keys()))
            not_received = sorted(
                set(pkt_sent[node_id].keys()) - set(pkt_received[node_id].keys()))
            pdr = float(len(pkt_received[node_id]))/len(pkt_sent[node_id])*100
            dels = []
            for pkt_id in pkt_id_list:
                dels.append(pkt_received[node_id]
                            [pkt_id] - pkt_sent[node_id][pkt_id])
            delays[node_id] = (dels, not_received, pdr)

        else:
            delays[node_id] = ([], sorted(pkt_sent[node_id].keys()), 0.0)

    total_time = 0
    counter = 0
    average_pdr = 0
    for i in delays.keys():
        total_time += sum(delays[i][0])
        counter += len(delays[i][0])
        average_pdr += delays[i][2]
    average_pdr = average_pdr/len(delays.keys())

    print("\n*********** Statistics for Network *************")
    print("Average Delay: {:.2f} ms".format(total_time/counter))
    print("Packet Delivery Ratio: {:.2f}%".format(average_pdr))

    print("\n********* Average Delays for each Node *********")
    for i in sorted(delays.keys()):
        if len(delays[i][0]) != 0:
            print("Node {}: {:.2f} ms".format(i, sum(delays[i][0])/len(delays[i][0])))
        else:
            print("Node {}: No packet received -> No Delay".format(i))
        print("Packet IDs not received: {}".format(delays[i][1]))
        print("Packet Delivery Ratio: {:.2f}%".format(delays[i][2]))
        print('-' * 50)
    print("Number of Active Nodes:", len(pkt_sent))
    print("Active nodes:", sorted(pkt_sent.keys()))


if __name__ == "__main__":
    main()
