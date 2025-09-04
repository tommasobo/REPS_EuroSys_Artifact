import plotly.express as px
import pandas as pd
from pathlib import Path
import plotly.graph_objs as go
import plotly
from plotly.subplots import make_subplots
from datetime import datetime
import os
import re
import natsort 
from argparse import ArgumentParser
import warnings
warnings.filterwarnings("ignore")

# Parameters
skip_small_value = True
ECN = True

def main(args):

    # RTT Data
    colnames=['Time', 'RTT', 'seqno', 'ackno', 'base', 'target']
    df = pd.DataFrame(columns =['Time','RTT', 'seqno', 'ackno', 'base', 'target'])
    name = ['0'] * df.shape[0]
    df = df.assign(Node=name)

    pathlist = Path('../sim/datacenter/rtt').glob('**/*.txt')
    for files in sorted(pathlist):
        path_in_str = str(files)
        temp_df = pd.read_csv(path_in_str, names=colnames, header=None, index_col=False, sep=',')
        name = [str(path_in_str)] * temp_df.shape[0]
        temp_df = temp_df.assign(Node=name)
        df = pd.concat([df, temp_df])

    base_rtt = df["base"].max()
    target_rtt = df["target"].max()

    if (len(df) > 100000):
        ratio = len(df) / 100000
        # DownScale
        df = df.iloc[::int(ratio)]
        # Reset the index of the new dataframe
        df.reset_index(drop=True, inplace=True)


    # Queue Data
    colnames=['Time', 'Queue'] 
    df_q = pd.DataFrame(columns=colnames)
    name = ['0'] * df_q.shape[0]
    df_q = df_q.assign(Node=name)

    pathlist = Path('../sim/datacenter/queueSize').glob('**/*.txt')
    for files in sorted(pathlist):
        path_in_str = str(files)
        temp_df_q = pd.read_csv(path_in_str, names=colnames, header=None, index_col=False, sep=',')
        name = [str(path_in_str)] * temp_df_q.shape[0]
        temp_df_q = temp_df_q.assign(Node=name)
        df_q = pd.concat([df_q, temp_df_q])

    if (len(df_q) > 100000):
        ratio = len(df_q) / 100000
        # DownScale
        df_q = df_q.iloc[::int(ratio)]
        # Reset the index of the new dataframe
        df_q.reset_index(drop=True, inplace=True)

    # Cwd Data
    colnames=['Time', 'Congestion Window'] 
    df2 = pd.DataFrame(columns =colnames)
    name = ['0'] * df2.shape[0]
    df2 = df2.assign(Node=name)
    df2.drop_duplicates('Time', inplace = True)

    pathlist = Path('../sim/datacenter/cwd').glob('**/*.txt')
    for files in sorted(pathlist):
        path_in_str = str(files)
        temp_df2 = pd.read_csv(path_in_str, names=colnames, header=None, index_col=False, sep=',')
        name = [str(path_in_str)] * temp_df2.shape[0]
        temp_df2 = temp_df2.assign(Node=name)
        temp_df2.drop_duplicates('Time', inplace = True)
        df2 = pd.concat([df2, temp_df2])
    if (len(df2) > 100000):
        ratio = len(df2) / 100000
        # DownScale
        df2 = df2.iloc[::int(ratio)]
        # Reset the index of the new dataframe
        df2.reset_index(drop=True, inplace=True)

    # ECN Data
    colnames=['Time', 'ECN'] 
    df4 = pd.DataFrame(columns =colnames)
    name = ['0'] * df4.shape[0]
    df4 = df4.assign(Node=name)

    pathlist = Path('../sim/datacenter/ecn').glob('**/*.txt')
    for files in sorted(pathlist):
        path_in_str = str(files)
        temp_df4 = pd.read_csv(path_in_str, names=colnames, header=None, index_col=False, sep=',')
        name = [str(path_in_str)] * temp_df4.shape[0]
        temp_df4 = temp_df4.assign(Node=name)
        df4 = pd.concat([df4, temp_df4])
    if (len(df4) > 100000):
        ratio = len(df) / 100000
        # DownScale
        if (ratio < 1):
            ratio = 1
        df4 = df4.iloc[::int(ratio)]
        # Reset the index of the new dataframe
        df4.reset_index(drop=True, inplace=True)
    
    # Nack data
    colnames=['Time', 'Nack'] 
    df6 = pd.DataFrame(columns =colnames)
    name = ['0'] * df6.shape[0]
    df6 = df6.assign(Node=name)
    

    pathlist = Path('../sim/datacenter/nack').glob('**/*.txt')
    for files in sorted(pathlist):
        path_in_str = str(files)
        temp_df6 = pd.read_csv(path_in_str, names=colnames, header=None, index_col=False, sep=',')
        name = [str(path_in_str)] * temp_df6.shape[0]
        temp_df6 = temp_df6.assign(Node=name)
        df6 = pd.concat([df6, temp_df6])

    if (len(df6) > 100000):
        ratio = len(df6) / 100000
        # DownScale
        if (ratio <= 1):
            ratio = int(1)
        print(ratio)
        df6 = df6.iloc[::int(ratio)]
        # Reset the index of the new dataframe
        df6.reset_index(drop=True, inplace=True)

    # Dropped data
    colnames=['Time', 'Dropped'] 
    df_dropped = pd.DataFrame(columns =colnames)
    name = ['0'] * df_dropped.shape[0]
    df_dropped = df_dropped.assign(Node=name)
    

    pathlist = Path('../sim/datacenter/dropped').glob('**/*.txt')
    for files in sorted(pathlist):
        path_in_str = str(files)
        temp_df_dropped = pd.read_csv(path_in_str, names=colnames, header=None, index_col=False, sep=',')
        name = [str(path_in_str)] * temp_df_dropped.shape[0]
        temp_df_dropped = temp_df_dropped.assign(Node=name)
        df_dropped = pd.concat([df_dropped, temp_df_dropped])

    if (len(df_dropped) > 100000):
        ratio = len(df_dropped) / 100000
        # DownScale
        if (ratio <= 1):
            ratio = int(1)
        print(ratio)
        df_dropped = df_dropped.iloc[::int(ratio)]
        # Reset the index of the new dataframe
        df_dropped.reset_index(drop=True, inplace=True)

    print("Finished Parsing")
    

    # Create figure with secondary y-axis
    fig = make_subplots(specs=[[{"secondary_y": True}]])
    color = ['#636EFA', '#0511a9', '#EF553B', '#00CC96', '#AB63FA', '#FFA15A', '#19D3F3', '#FF6692', '#B6E880', '#FF97FF', '#FECB52']   
    # CWND
    for i in df2['Node'].unique():
        sub_df = df2.loc[df2['Node'] == str(i)]
        fig.add_trace(
            go.Scatter(x=sub_df["Time"], y=sub_df['Congestion Window'], name="CWD " + str(i), line=dict(dash='dot'), showlegend=True),
            secondary_y=True,
        )

    # RTT
    for i in df['Node'].unique():
        sub_df = df.loc[df['Node'] == str(i)]
        fig.add_trace(
            go.Scatter(x=sub_df["Time"], y=sub_df['RTT'], mode='markers', marker=dict(size=2), name=str(i), line=dict(color=color[0]), opacity=0.9, showlegend=True),
            secondary_y=False,
        )


    # Queue
    for i in df_q['Node'].unique():
        sub_df_q = df_q.loc[df_q['Node'] == str(i)]

        if (sub_df_q["Queue"].max() < 500):
            continue

        fig.add_trace(
            go.Scatter(x=sub_df_q["Time"], y=sub_df_q['Queue'], mode='markers', marker=dict(size=2), name=str(i),line=dict(dash='dash', color="black", width=3), opacity=0.9, showlegend=True),
            secondary_y=False,
        )

    
    # ECN
    print("ECN Plot")
    mean_ecn = df4["Time"].mean()
    for i in df4['Node'].unique():
        df4['ECN'] = 10000
        sub_df4 = df4.loc[df4['Node'] == str(i)]
        fig.add_trace(
            go.Scatter(x=sub_df4["Time"], y=sub_df4['ECN'], mode="markers", marker_symbol="triangle-up", name="ECN Packet", marker=dict(size=5, color="yellow"), showlegend=True),
            secondary_y=False
        )

    # NACK
    print("NACK Plot")
    mean_sent = df6["Time"].mean()
    df6['Nack'] = df6['Nack'].multiply(15000)
    for i in df6['Node'].unique():
        sub_df6 = df6.loc[df6['Node'] == str(i)]
        fig.add_trace(
            go.Scatter(x=sub_df6["Time"], y=sub_df6["Nack"], mode="markers", marker_symbol="triangle-up", name="NACK Packet", marker=dict(size=5, color="grey"), showlegend=True),
            secondary_y=False
        )

    # Dropped
    print("Dropped Plot")
    mean_sent = df_dropped["Time"].mean()
    df_dropped['Dropped'] = df_dropped['Dropped'].multiply(8000)
    for i in df_dropped['Node'].unique():
        sub_df_dropped = df_dropped.loc[df_dropped['Node'] == str(i)]
        fig.add_trace(
            go.Scatter(x=sub_df_dropped["Time"], y=sub_df_dropped["Dropped"], mode="markers", marker_symbol="triangle-up", name="NACK Packet", marker=dict(size=5, color="black"), showlegend=True),
            secondary_y=False
        )

    config = {
    'toImageButtonOptions': {
        'format': 'png', # one of png, svg, jpeg, webp
        'filename': 'custom_image',
        'height': 550,
        'width': 1000,
        'scale':4 # Multiply title/legend/axis/canvas sizes by this factor
    }
    }

    # Set x-axis title
    fig.update_xaxes(title_text="Time (ns)")
    # Set y-axes titles
    fig.update_yaxes(title_text="RTT || Queuing Latency (ns)", secondary_y=False)
    fig.update_yaxes(title_text="Congestion Window (B)", secondary_y=True)
    
    now = datetime.now() # current date and time
    date_time = now.strftime("%m:%d:%Y_%H:%M:%S")
    print("Saving Plot")
    #fig.write_image("out/fid_simple_{}.png".format(date_time))
    #plotly.offline.plot(fig, filename='out/fid_simple_{}.html'.format(date_time))
    print("Showing Plot")
    if (args.output_folder is not None):
        plotly.offline.plot(fig, filename=args.output_folder + "/{}.html".format(args.name))
    if (args.no_show is None):
        fig.show()

if __name__ == "__main__":
    parser = ArgumentParser()
    parser.add_argument("--x_limit", type=int, help="Max X Value showed", default=None)
    parser.add_argument("--y_limit", type=int, help="Max Y value showed", default=None)
    parser.add_argument("--show_ecn", type=str, help="Show ECN points", default=None)
    parser.add_argument("--show_sent", type=str, help="Show Sent Points", default=None) 
    parser.add_argument("--show_triangles", type=str, help="Show RTT triangles", default=None) 
    parser.add_argument("--num_to_show", type=int, help="Number of lines to show", default=None) 
    parser.add_argument("--annotations", type=str, help="Number of lines to show", default=None) 
    parser.add_argument("--output_folder", type=str, help="OutFold", default=None) 
    parser.add_argument("--input_file", type=str, help="InFold", default=None) 
    parser.add_argument("--name", type=str, help="Name Algo", default=None) 
    parser.add_argument("--no_show", type=int, help="Don't show plot, just save", default=None) 
    parser.add_argument("--show_case", type=int, help="ShowCases", default=None) 
    parser.add_argument("--cumulative_case", type=int, help="Do it cumulative", default=None) 
    args = parser.parse_args()
    main(args)
