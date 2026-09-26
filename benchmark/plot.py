import pandas as pd
import matplotlib.pyplot as plt
import os

results_dir = '../Results'

# Part 1
try:
    df = pd.read_csv(f'{results_dir}/Part1_throughput.csv')
    plt.figure()
    plt.plot(df['Clients'], df['Throughput_MBps'], marker='o')
    plt.title('Part 1: Throughput vs Number of Clients')
    plt.xlabel('Number of Clients')
    plt.ylabel('Throughput (MB/s)')
    plt.grid(True)
    plt.savefig(f'{results_dir}/Part1_throughput.png')
except Exception as e:
    print(e)

# Part 2 Cases 1 & 2
try:
    df = pd.read_csv(f'{results_dir}/Part2_throughput.csv')
    plt.figure()
    plt.plot(df['FailProb'], df['Case1_Throughput_MBps'], marker='o', label='Case 1 (No Session)')
    plt.plot(df['FailProb'], df['Case2_Throughput_MBps'], marker='s', label='Case 2 (Session Mgmt)')
    plt.title('Part 2: Throughput vs Failure Probability')
    plt.xlabel('Failure Probability')
    plt.ylabel('Throughput (MB/s)')
    plt.legend()
    plt.grid(True)
    plt.savefig(f'{results_dir}/Part2_Cases_throughput.png')
except Exception as e:
    print(e)

# Part 2 Enhanced
try:
    df = pd.read_csv(f'{results_dir}/Part2_Enhanced_throughput.csv')
    plt.figure()
    plt.plot(df['Connections'], df['Throughput_MBps'], marker='^', color='green')
    plt.title('Part 2 Enhanced: Throughput vs Parallel Connections (Fail=0.05)')
    plt.xlabel('Number of Connections')
    plt.ylabel('Throughput (MB/s)')
    plt.grid(True)
    plt.savefig(f'{results_dir}/Part2_Enhanced_throughput.png')
except Exception as e:
    print(e)

print("Graphs generated.")
