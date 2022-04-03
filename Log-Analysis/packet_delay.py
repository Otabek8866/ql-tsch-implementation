# Input file name
from curses import keyname


INPUT_FILE = "loglistener.txt"
# number of nodes that have been simulated
MAX_NODE_NUMBER = 3
# delays for each node udp transmissions -> 2D matrix
delays = {}
node_udp_communications = {}
sent_and_received = {}

def main():
    
    with open(INPUT_FILE, "r") as f:
        for line in f:
            if not (("INFO: App" in line) and ("Send_to" in line or "Received_from" in line)):
                continue    
            fields = line.split()
            id = int(fields[1][3:])
            if id == 1:
                id = int(fields[7])
            if id in node_udp_communications:
                node_udp_communications[id].append(fields)
            else:
                node_udp_communications[id] = [fields]
    
    for i in range(2, MAX_NODE_NUMBER + 1):
        sent_and_received[i] = {'sent': [], 'delivered':[]}
        delays[i] = []
        node_udp_communications[i].sort(key = lambda x: x[0])
        j = 0
        for _ in range(len(node_udp_communications[i])//2):
            if "Send_to" in node_udp_communications[i][j]:
                
    

    



if __name__ == "__main__":
    main()