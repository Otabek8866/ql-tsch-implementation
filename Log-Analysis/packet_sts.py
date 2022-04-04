INPUT_FILE = "loglistener.txt"
# number of nodes that have been simulated
MAX_NODE_NUMBER = 0
# delays for each node udp transmissions -> 2D matrix
delays = {}
node_udp_com = {}
sent_and_received = {}


def main():
    global MAX_NODE_NUMBER
    with open(INPUT_FILE, "r") as f:
        for line in f:
            if not (("INFO: App" in line) and ("Sent_to" in line or "Received_from" in line)):
                continue
            fields = line.split()
            id = int(fields[1][3:])
            if id == 1:
                id = int(fields[6])
            if id in node_udp_com:
                node_udp_com[id].append(fields)
            else:
                node_udp_com[id] = [fields]
                MAX_NODE_NUMBER += 1

    for i in range(2, MAX_NODE_NUMBER + 2):
        sent_and_received[i] = {'sent': [], 'delivered': []}
        delays[i] = []
        node_udp_com[i].sort(key=lambda x: (int(x[0]), int(x[-1])))
        j = 0
        while j < len(node_udp_com[i])-1:
            if node_udp_com[i][j+1][5] == "Received_from":
                if node_udp_com[i][j][-1] == node_udp_com[i][j+1][-1]:
                    delays[i].append(
                        int(node_udp_com[i][j+1][0]) - int(node_udp_com[i][j][0]))
                    sent_and_received[i]['sent'].append(
                        int(node_udp_com[i][j][-1]))
                    sent_and_received[i]['delivered'].append(
                        int(node_udp_com[i][j+1][-1]))
                    j += 2
                else:
                    sent_and_received[i]['sent'].append(
                        int(node_udp_com[i][j][-1]))
                    j += 1
            else:
                sent_and_received[i]['sent'].append(
                    int(node_udp_com[i][j][-1]))
                j += 1

    total_average_delay = 0
    counter = 0
    for i in range(2, MAX_NODE_NUMBER + 2):
        total_average_delay += sum(delays[i])
        counter += len(delays[i])

    total_sent = 0
    total_delivered = 0
    for i in range(2, MAX_NODE_NUMBER + 2):
        total_sent += len(sent_and_received[i]['sent'])
        total_delivered += len(sent_and_received[i]['delivered'])

    print("\n*********** Statistics for Network *************")
    print("Average Delay: {:.2f} ms".format(total_average_delay/counter))
    print("Packet Delivery Ratio: {:.2f}%".format(
        (total_delivered/total_sent)*100))

    print("\n********* Average Delays for each Node *********")
    for i in range(2, MAX_NODE_NUMBER + 2):
        print("Node {}: {:.2f} ms".format(i, sum(delays[i])/len(delays[i])))

    print("\n********* Packet Delivery Ratio *********")
    for i in range(2, MAX_NODE_NUMBER + 2):
        print("Node {}: {:.2f}".format(i, (len(
            sent_and_received[i]['delivered'])/len(sent_and_received[i]['sent']))*100), end='\t')
        print("Undeliivered Packets: {}, Packet indexes: {}".format(
            len(sent_and_received[i]['sent']) - len(sent_and_received[i]['delivered']), list(set(sent_and_received[i]['sent']) - set(sent_and_received[i]['delivered']))))


if __name__ == "__main__":
    main()
