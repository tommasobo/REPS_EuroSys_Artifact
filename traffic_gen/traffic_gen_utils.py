""" Utility functions for the traffic generator. """

import math
import random

CHANGING_DST = 0.8
DSTS = None
LIST_BUILT = False
connections_map = {}

def translate_bandwidth(bandwidth_string: str) -> float:
    """
    Converts a bandwidth string with units (K, M, G) to its value in bits per second.

    Args:
        bandwidth_string (str): The bandwidth string to convert.
                                Should end with 'K', 'M', or 'G' to denote the units.

    Returns:
        float: The bandwidth value in bits per second.

    Raises:
        TypeError: If the input is not a string.
        ValueError: If the input string is not a valid bandwidth format.
    """

    if not isinstance(bandwidth_string, str):
        raise TypeError("Input must be a string")

    unit_multiplier = {"K": 1e3, "M": 1e6, "G": 1e9}
    unit = bandwidth_string[-1]
    if unit.isnumeric():
        multiplier = 1
    elif unit in unit_multiplier:
        multiplier = unit_multiplier[unit]
    else:
        raise ValueError(f"Invalid unit format: {unit}. Must be K, M, or G.")

    value = bandwidth_string[:-1]
    try:
        return float(value) * multiplier
    except ValueError as e:
        raise ValueError(f"Invalid bandwidth format: {bandwidth_string}") from e


def exponential_dist_sample(mean: float) -> float:
    """
    Generates a random number from an exponential distribution with a given mean
    (mean = 1 / lambda).

    Args:
        mean (float): The mean of the exponential distribution (mean = 1 / lambda).
        This mean signifies the average time between events in a Poisson process.

    Returns:
        float: A random number sampled from an exponential distribution with the given mean.
    """
    return -math.log(1 - random.random()) * mean

import random


def generate_simple_traffic_matrix(num_nodes=128, nodes_per_switch=8):
    nodes = list(range(num_nodes))
    global connections_map
    
    # Shuffle the nodes to get random receivers
    receivers = nodes.copy()
    random.shuffle(receivers)
    
    for sender in nodes:
        # Ensure sender does not send to itself
        if sender == receivers[-1]:
            # If the last receiver is the same as the sender, swap with another
            swap_with = random.choice([r for r in receivers if r != sender])
            swap_index = receivers.index(swap_with)
            receivers[-1], receivers[swap_index] = receivers[swap_index], receivers[-1]
        
        receiver = receivers.pop()
        connections_map[sender] = receiver

def generate_traffic_matrix(num_nodes=128, nodes_per_switch=8):
    # Initialize all nodes and switches
    nodes = list(range(num_nodes))
    switches = {i: nodes[i * nodes_per_switch: (i + 1) * nodes_per_switch] for i in range(num_nodes // nodes_per_switch)}

    # Function to find nodes not under the same switch
    def get_valid_receivers(sender, switches):
        sender_switch = sender // nodes_per_switch
        valid_receivers = []
        for switch, switch_nodes in switches.items():
            if switch != sender_switch:
                valid_receivers.extend(switch_nodes)
        return valid_receivers

    # Dictionary to store SRC -> DEST pairs
    global connections_map

    # Keep track of available receivers
    available_receivers = set(nodes)
    
    # Shuffle the nodes to randomize sender processing order
    random.shuffle(nodes)

    for sender in nodes:
        # Get valid receivers (not in the same switch)
        valid_receivers = list(set(get_valid_receivers(sender, switches)) & available_receivers)

        if valid_receivers:
            # Randomly pick a receiver from another switch
            receiver = random.choice(valid_receivers)
        else:
            # Fallback: pick a receiver from the same switch if no other option
            same_switch_receivers = list(available_receivers & set(switches[sender // nodes_per_switch]))
            if not same_switch_receivers:
                raise ValueError(f"No available valid receivers even within the same switch for sender {sender}")
            receiver = random.choice(same_switch_receivers)

        # Assign the receiver and remove it from available list
        connections_map[sender] = receiver
        available_receivers.remove(receiver)


def get_dst(src_idx: int, number_hosts: int) -> int:
    """Get a random destination host index that is not the same as the source host index.

    Args:
        src_idx (int): The source host index.
        number_hosts (int): The number of hosts.
    Returns:
        int: The destination host index.
    """
    if (False):
        global DSTS
        if (DSTS is None):
            DSTS = [-1] * number_hosts

        if DSTS[src_idx] != -1 and DSTS[src_idx] != -2:
            return DSTS[src_idx]
        
    global LIST_BUILT
    global connections_map
    if (not LIST_BUILT):
        generate_traffic_matrix(number_hosts, 8)
        LIST_BUILT = True

    dst_idx = random.randint(0, number_hosts - 1)
    # Ensure sender is not the same as receiver. or int(dst_idx / 8) == int(src_idx / 8) 
    while dst_idx == src_idx :
        dst_idx = random.randint(0, number_hosts - 1)

    """ dst_idx = (src_idx + 8) % number_hosts """
    if False:
        if (DSTS[src_idx] != -2):
            if random.uniform(0, 1) < CHANGING_DST:
                
                DSTS[src_idx] = dst_idx
            else:
                DSTS[src_idx] = -2

    if (random.randint(0, 100) < 5):
        return dst_idx
    elif LIST_BUILT:
        dst_idx = connections_map[src_idx]

    return dst_idx
