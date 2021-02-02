#! /usr/bin/env python3
# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2021 Intel Corporation

"""
Script to automate running crypto performance tests for a range of test
cases as configured in the JSON file specified by the user.
The results are processed and output into various graphs in PDF files.
Currently, throughput and latency tests are supported.
"""

import glob
import json
import os
import shutil
import subprocess
from argparse import ArgumentParser
from argparse import ArgumentDefaultsHelpFormatter
import img2pdf
import pandas as pd
import plotly.express as px

SCRIPT_PATH = os.path.dirname(__file__) + "/"
GRAPH_DIR = "temp_graphs"


class Grapher:
    """Grapher object containing all graphing functions. """
    def __init__(self, config, suite, graph_path):
        self.graph_num = 0
        self.graph_path = graph_path
        self.suite = suite
        self.config = config
        self.test = ""
        self.ptest = ""
        self.data = pd.DataFrame()

    def save_graph(self, fig, subdir):
        """
        Update figure layout to increase readability, output to JPG file.
        """
        path = os.path.join(self.graph_path, subdir, "")
        if not os.path.exists(path):
            os.makedirs(path)
        fig.update_layout(font_size=30, title_x=0.5, title_font={"size": 25},
                          margin={'t': 300, 'l': 150, 'r': 150, 'b': 150})
        fig.write_image(path + "%d.jpg" % self.graph_num)

    def boxplot_graph(self, x_axis_label, burst, buffer):
        """Plot a boxplot graph for the given parameters."""
        fig = px.box(self.data, x=x_axis_label,
                     title="Config: " + self.config + "<br>Test Suite: " +
                     self.suite + "<br>" + self.test +
                     "<br>(Outliers Included)<br>Burst Size: " + burst +
                     ", Buffer Size: " + buffer,
                     height=1400, width=2400)
        self.save_graph(fig, x_axis_label.replace(' ', '_'))
        self.graph_num += 1

    def grouped_graph(self, y_axis_label, x_axis_label, color_label):
        """Plot a grouped barchart using the given parameters."""
        if (self.data[y_axis_label] == 0).all():
            return
        fig = px.bar(self.data, x=x_axis_label, color=color_label,
                     y=y_axis_label,
                     title="Config: " + self.config + "<br>Test Suite: " +
                     self.suite + "<br>" + self.test + "<br>"
                     + y_axis_label + " for each " + x_axis_label +
                     "/" + color_label, barmode="group", height=1400,
                     width=2400)
        fig.update_xaxes(type='category')
        self.save_graph(fig, y_axis_label.replace(' ', '_'))
        self.graph_num += 1

    def histogram_graph(self, x_axis_label, burst, buffer):
        """Plot a histogram graph using the given parameters."""
        quart1 = self.data[x_axis_label].quantile(0.25)
        quart3 = self.data[x_axis_label].quantile(0.75)
        inter_quart_range = quart3 - quart1
        data_out = self.data[~((self.data[x_axis_label] <
                                (quart1 - 1.5 * inter_quart_range)) |
                               (self.data[x_axis_label] >
                                (quart3 + 1.5 * inter_quart_range)))]
        fig = px.histogram(data_out, x=x_axis_label,
                           title="Config: " + self.config + "<br>Test Suite: "
                           + self.suite + "<br>" + self.test
                           + "<br>(Outliers removed using Interquartile Range)"
                           + "<br>Burst Size: " + burst + ", Buffer Size: " +
                           buffer, height=1400, width=2400)
        max_val = data_out[x_axis_label].max()
        min_val = data_out[x_axis_label].min()
        fig.update_traces(xbins=dict(
            start=min_val,
            end=max_val,
            size=(max_val - min_val) / 200
        ))
        self.save_graph(fig, x_axis_label.replace(' ', '_'))
        self.graph_num += 1


def cleanup_throughput_datatypes(data):
    """Cleanup data types of throughput test results dataframe. """
    data.columns = data.columns.str.replace('/', ' ')
    data.columns = data.columns.str.strip()
    data['Burst Size'] = data['Burst Size'].astype('category')
    data['Buffer Size(B)'] = data['Buffer Size(B)'].astype('category')
    data['Failed Enq'] = data['Failed Enq'].astype('int')
    data['Throughput(Gbps)'] = data['Throughput(Gbps)'].astype('float')
    data['Ops(Millions)'] = data['Ops(Millions)'].astype('float')
    data['Cycles Buf'] = data['Cycles Buf'].astype('float')
    return data


def cleanup_latency_datatypes(data):
    """Cleanup data types of latency test results dataframe. """
    data.columns = data.columns.str.strip()
    data = data[['Burst Size', 'Buffer Size', 'time (us)']].copy()
    data['Burst Size'] = data['Burst Size'].astype('category')
    data['Buffer Size'] = data['Buffer Size'].astype('category')
    data['time (us)'] = data['time (us)'].astype('float')
    return data


def process_test_results(grapher, data):
    """
    Process results from the test case,
    calling graph functions to output graph images.
    """
    if grapher.ptest == "throughput":
        grapher.data = cleanup_throughput_datatypes(data)
        for y_label in ["Throughput(Gbps)", "Ops(Millions)",
                        "Cycles Buf", "Failed Enq"]:
            grapher.grouped_graph(y_label, "Buffer Size(B)",
                                  "Burst Size")
    elif grapher.ptest == "latency":
        clean_data = cleanup_latency_datatypes(data)
        for (burst, buffer), group in clean_data.groupby(['Burst Size',
                                                          'Buffer Size']):
            grapher.data = group
            grapher.histogram_graph("time (us)", burst, buffer)
            grapher.boxplot_graph("time (us)", burst, buffer)
    else:
        print("Invalid ptest")
        return


def create_results_pdf(graph_path, pdf_path):
    """Output results graphs to PDFs."""
    if not os.path.exists(pdf_path):
        os.makedirs(pdf_path)
    for _, dirs, _ in os.walk(graph_path):
        for sub in dirs:
            graphs = sorted(glob.glob(os.path.join(graph_path, sub, "*.jpg")),
                            key=(lambda x: int((x.rsplit('/', 1)[1])
                                               .split('.')[0])))
            if graphs:
                with open(pdf_path + "%s_results.pdf" % sub, "wb") as pdf_file:
                    pdf_file.write(img2pdf.convert(graphs))


def run_test(test_cmd, test, grapher, params, verbose):
    """Run performance test app for the given test case parameters."""
    process = subprocess.Popen(["stdbuf", "-oL", test_cmd] + params,
                               universal_newlines=True,
                               stdout=subprocess.PIPE,
                               stderr=subprocess.STDOUT)
    rows = []
    if verbose:
        print("\n\tOutput for " + test + ":")
    while process.poll() is None:
        line = process.stdout.readline().strip()
        if not line:
            continue
        if verbose:
            print("\t\t>>" + line)

        if line.replace(' ', '').startswith('#lcore'):
            columns = line[1:].split(',')
        elif line[0].isdigit():
            line = line.replace(';', ',')
            rows.append(line.split(','))
        else:
            continue

    if process.poll() != 0 or not columns or not rows:
        print("\n\t" + test + ": FAIL")
        return
    data = pd.DataFrame(rows, columns=columns)
    grapher.test = test
    process_test_results(grapher, data)
    print("\n\t" + test + ": OK")
    return


def parse_parameters(config_parameters):
    """Convert the JSON config to list of strings."""
    params = []
    for (key, val) in config_parameters:
        if isinstance(val, bool):
            params.append("--" + key if val is True else "")
        elif len(key) == 1:
            params.append("-" + key)
            params.append(val)
        else:
            params.append("--" + key + "=" + val)
    return params


def run_test_suite(test_cmd, suite_config, verbose):
    """Parse test cases for the test suite and run each test."""
    print("\nRunning Test Suite: " + suite_config['suite'])
    graph_path = os.path.join(suite_config['output_path'], GRAPH_DIR,
                              suite_config['suite'], "")
    grapher = Grapher(suite_config['config_name'], suite_config['suite'],
                      graph_path)
    test_cases = suite_config['test_cases']
    if 'default' not in test_cases:
        print("Test Suite must contain default case, skipping")
        return

    default_params = parse_parameters(test_cases['default']['eal'].items())
    default_params.append("--")
    default_params += parse_parameters(test_cases['default']['app'].items())

    if 'ptest' not in test_cases['default']['app']:
        print("Test Suite must contain default ptest value, skipping")
        return
    grapher.ptest = test_cases['default']['app']['ptest']

    for (test, params) in {k: v for (k, v) in test_cases.items() if
                           k != "default"}.items():
        extra_params = parse_parameters(params.items())
        run_test(test_cmd, test, grapher, default_params + extra_params,
                 verbose)

    create_results_pdf(graph_path, os.path.join(suite_config['output_path'],
                                                suite_config['suite'], ""))


def parse_args():
    """Parse command-line arguments passed to script."""
    parser = ArgumentParser(formatter_class=ArgumentDefaultsHelpFormatter)
    parser.add_argument('config_path', type=str,
                        help="Path to JSON configuration file")
    parser.add_argument('-t', '--test-suites', nargs='+', default=["all"],
                        help="List of test suites to run")
    parser.add_argument('-v', '--verbose', action='store_true',
                        help="""Display perf test app output.
                        Not recommended for latency tests.""")
    parser.add_argument('-f', '--file-path',
                        default=shutil.which('dpdk-test-crypto-perf'),
                        help="Path for perf test app")
    parser.add_argument('-o', '--output-path', default=SCRIPT_PATH,
                        help="Path to store output directories")
    args = parser.parse_args()
    return (args.file_path, args.test_suites, args.config_path,
            args.output_path, args.verbose)


def main():
    """
    Load JSON config and call relevant functions to run chosen test suites.
    """
    test_cmd, test_suites, config_file, output_path, verbose = parse_args()
    if test_cmd is None or not os.path.isfile(test_cmd):
        print("Invalid filepath for perf test app!")
        return
    try:
        with open(config_file) as conf:
            test_suite_ops = json.load(conf)
            config_name = os.path.splitext(config_file)[0]
            if '/' in config_name:
                config_name = config_name.rsplit('/', 1)[1]
            output_path = os.path.join(output_path, config_name, "")
            print("Using config: " + config_file)
    except OSError as err:
        print("Error with JSON file path: " + err.strerror)
        return
    except json.decoder.JSONDecodeError as err:
        print("Error loading JSON config: " + err.msg)
        return

    if test_suites != ["all"]:
        suite_list = []
        for (suite, test_cases) in {k: v for (k, v) in test_suite_ops.items()
                                    if k in test_suites}.items():
            suite_list.append(suite)
            suite_config = {'config_name': config_name, 'suite': suite,
                            'test_cases': test_cases,
                            'output_path': output_path}
            run_test_suite(test_cmd, suite_config, verbose)
        if not suite_list:
            print("No valid test suites chosen!")
            return
    else:
        for (suite, test_cases) in test_suite_ops.items():
            suite_config = {'config_name': config_name, 'suite': suite,
                            'test_cases': test_cases,
                            'output_path': output_path}
            run_test_suite(test_cmd, suite_config, verbose)

    graph_path = os.path.join(output_path, GRAPH_DIR, "")
    if os.path.exists(graph_path):
        shutil.rmtree(graph_path)


if __name__ == "__main__":
    main()
