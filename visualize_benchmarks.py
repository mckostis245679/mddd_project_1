#!/usr/bin/env python3
"""
Visualize comprehensive benchmark results for multidimensional data structures.
Creates grouped bar charts showing performance across different k values and query types.
"""

import pandas as pd
import matplotlib.pyplot as plt
import numpy as np
from pathlib import Path

def load_benchmark_data(csv_path=None):
    """Load benchmark data from CSV file. Auto-detects if not specified."""
    if csv_path is None:
        # Try to find the CSV file automatically
        if Path('comprehensive_benchmark.csv').exists():
            csv_path = 'comprehensive_benchmark.csv'
            print("Using: comprehensive_benchmark.csv (full dataset)")
        elif Path('comprehensive_benchmark_sample.csv').exists():
            csv_path = 'comprehensive_benchmark_sample.csv'
            print("Using: comprehensive_benchmark_sample.csv (sample dataset)")
        else:
            raise FileNotFoundError("No benchmark CSV file found!")

    df = pd.read_csv(csv_path)
    return df

def create_grouped_bar_chart(df, query_type, output_path=None):
    """
    Create a grouped bar chart for a specific query type.
    Shows all structures grouped by k value.
    """
    # Filter for specific query type
    data = df[df['query_type'] == query_type].copy()

    if data.empty:
        print(f"No data for query type: {query_type}")
        return

    # Get unique k values and structures
    k_values = sorted(data['k'].unique())
    structures = sorted(data['structure'].unique())

    # Colors for different structures
    colors = {
        'KDTree': '#3498db',      # Blue
        'RTree': '#e74c3c',       # Red
        'QuadTree': '#2ecc71',    # Green
        'RangeTree2D': '#f39c12', # Orange
        'LSH': '#9b59b6'          # Purple
    }

    # Create figure
    fig, ax = plt.subplots(figsize=(12, 6))

    # Set width of bars and positions
    bar_width = 0.15
    x_positions = np.arange(len(k_values))

    # Plot bars for each structure
    for i, structure in enumerate(structures):
        struct_data = data[data['structure'] == structure]

        # Get mean query time for each k
        means = []
        for k in k_values:
            k_data = struct_data[struct_data['k'] == k]
            if not k_data.empty:
                means.append(k_data['mean_query_us'].values[0])
            else:
                means.append(0)  # Structure doesn't support this k

        # Plot bars
        offset = (i - len(structures) / 2) * bar_width + bar_width / 2
        bars = ax.bar(x_positions + offset, means, bar_width,
                     label=structure, color=colors.get(structure, '#95a5a6'))

        # Add value labels on top of bars
        for bar, mean in zip(bars, means):
            if mean > 0:
                height = bar.get_height()
                ax.text(bar.get_x() + bar.get_width() / 2., height,
                       f'{int(mean)}',
                       ha='center', va='bottom', fontsize=8)

    # Customize plot
    ax.set_xlabel('K (Dimensions)', fontsize=12, fontweight='bold')
    ax.set_ylabel('Time (μs)', fontsize=12, fontweight='bold')
    ax.set_title(f'{query_type.upper()} Query Performance', fontsize=14, fontweight='bold')
    ax.set_xticks(x_positions)
    ax.set_xticklabels([f'K={k}' if k > 0 else 'N/A' for k in k_values])
    ax.legend(loc='upper left', framealpha=0.9)
    ax.grid(axis='y', alpha=0.3, linestyle='--')

    plt.tight_layout()

    if output_path:
        plt.savefig(output_path, dpi=300, bbox_inches='tight')
        print(f"Saved: {output_path}")

    return fig

def create_build_time_chart(df, output_path=None):
    """Create a chart showing build times for all structures across k values."""
    # Get unique build time for each structure-k combination
    build_data = df.groupby(['structure', 'k'])['build_time_us'].first().reset_index()

    k_values = sorted(build_data['k'].unique())
    structures = sorted(build_data['structure'].unique())

    colors = {
        'KDTree': '#3498db',
        'RTree': '#e74c3c',
        'QuadTree': '#2ecc71',
        'RangeTree2D': '#f39c12',
        'LSH': '#9b59b6'
    }

    fig, ax = plt.subplots(figsize=(12, 6))

    bar_width = 0.15
    x_positions = np.arange(len(k_values))

    for i, structure in enumerate(structures):
        struct_data = build_data[build_data['structure'] == structure]

        build_times = []
        for k in k_values:
            k_data = struct_data[struct_data['k'] == k]
            if not k_data.empty:
                build_times.append(k_data['build_time_us'].values[0] / 1000.0)  # Convert to ms
            else:
                build_times.append(0)

        offset = (i - len(structures) / 2) * bar_width + bar_width / 2
        bars = ax.bar(x_positions + offset, build_times, bar_width,
                     label=structure, color=colors.get(structure, '#95a5a6'))

        # Add value labels
        for bar, time in zip(bars, build_times):
            if time > 0:
                height = bar.get_height()
                ax.text(bar.get_x() + bar.get_width() / 2., height,
                       f'{int(time)}',
                       ha='center', va='bottom', fontsize=8)

    ax.set_xlabel('K (Dimensions)', fontsize=12, fontweight='bold')
    ax.set_ylabel('Build Time (ms)', fontsize=12, fontweight='bold')
    ax.set_title('Tree Build Time Comparison', fontsize=14, fontweight='bold')
    ax.set_xticks(x_positions)
    ax.set_xticklabels([f'K={k}' if k > 0 else 'N/A' for k in k_values])
    ax.legend(loc='upper left', framealpha=0.9)
    ax.grid(axis='y', alpha=0.3, linestyle='--')

    plt.tight_layout()

    if output_path:
        plt.savefig(output_path, dpi=300, bbox_inches='tight')
        print(f"Saved: {output_path}")

    return fig

def create_comprehensive_comparison(df, output_path=None):
    """Create a 2x2 grid comparing all query types and build times."""
    fig, axes = plt.subplots(2, 2, figsize=(16, 12))
    fig.suptitle('Comprehensive Benchmark Results', fontsize=16, fontweight='bold')

    k_values = sorted([k for k in df['k'].unique() if k > 0])
    structures = sorted(df['structure'].unique())

    colors = {
        'KDTree': '#3498db',
        'RTree': '#e74c3c',
        'QuadTree': '#2ecc71',
        'RangeTree2D': '#f39c12',
        'LSH': '#9b59b6'
    }

    bar_width = 0.15

    # Plot for each query type
    query_types = ['knn', 'range', 'search']
    plot_configs = [
        (0, 0, 'knn', 'KNN Query'),
        (0, 1, 'range', 'Range Query'),
        (1, 0, 'search', 'Search Query'),
    ]

    for row, col, qtype, title in plot_configs:
        ax = axes[row, col]
        data = df[df['query_type'] == qtype]

        if not data.empty:
            x_positions = np.arange(len(k_values))

            for i, structure in enumerate(structures):
                struct_data = data[data['structure'] == structure]

                means = []
                for k in k_values:
                    k_data = struct_data[struct_data['k'] == k]
                    if not k_data.empty:
                        means.append(k_data['mean_query_us'].values[0])
                    else:
                        means.append(0)

                offset = (i - len(structures) / 2) * bar_width + bar_width / 2
                bars = ax.bar(x_positions + offset, means, bar_width,
                             label=structure, color=colors.get(structure, '#95a5a6'))

                # Add value labels
                for bar, mean in zip(bars, means):
                    if mean > 0:
                        height = bar.get_height()
                        ax.text(bar.get_x() + bar.get_width() / 2., height,
                               f'{int(mean)}',
                               ha='center', va='bottom', fontsize=7)

            ax.set_xlabel('K (Dimensions)', fontsize=10, fontweight='bold')
            ax.set_ylabel('Time (μs)', fontsize=10, fontweight='bold')
            ax.set_title(title, fontsize=12, fontweight='bold')
            ax.set_xticks(x_positions)
            ax.set_xticklabels([f'K={k}' for k in k_values])
            ax.legend(loc='upper left', fontsize=8, framealpha=0.9)
            ax.grid(axis='y', alpha=0.3, linestyle='--')

    # Build time plot
    ax = axes[1, 1]
    build_data = df.groupby(['structure', 'k'])['build_time_us'].first().reset_index()
    build_data = build_data[build_data['k'] > 0]  # Exclude LSH k=0

    x_positions = np.arange(len(k_values))

    for i, structure in enumerate(structures):
        struct_data = build_data[build_data['structure'] == structure]

        build_times = []
        for k in k_values:
            k_data = struct_data[struct_data['k'] == k]
            if not k_data.empty:
                build_times.append(k_data['build_time_us'].values[0] / 1000.0)  # ms
            else:
                build_times.append(0)

        offset = (i - len(structures) / 2) * bar_width + bar_width / 2
        bars = ax.bar(x_positions + offset, build_times, bar_width,
                     label=structure, color=colors.get(structure, '#95a5a6'))

        for bar, time in zip(bars, build_times):
            if time > 0:
                height = bar.get_height()
                ax.text(bar.get_x() + bar.get_width() / 2., height,
                       f'{int(time)}',
                       ha='center', va='bottom', fontsize=7)

    ax.set_xlabel('K (Dimensions)', fontsize=10, fontweight='bold')
    ax.set_ylabel('Build Time (ms)', fontsize=10, fontweight='bold')
    ax.set_title('Build Time', fontsize=12, fontweight='bold')
    ax.set_xticks(x_positions)
    ax.set_xticklabels([f'K={k}' for k in k_values])
    ax.legend(loc='upper left', fontsize=8, framealpha=0.9)
    ax.grid(axis='y', alpha=0.3, linestyle='--')

    plt.tight_layout()

    if output_path:
        plt.savefig(output_path, dpi=300, bbox_inches='tight')
        print(f"Saved: {output_path}")

    return fig

def print_summary_table(df):
    """Print a summary table of all benchmark results."""
    print("\n" + "="*80)
    print("BENCHMARK SUMMARY TABLE")
    print("="*80)

    # Group by structure and k
    for structure in sorted(df['structure'].unique()):
        struct_data = df[df['structure'] == structure]
        print(f"\n{structure}:")

        for k in sorted(struct_data['k'].unique()):
            k_data = struct_data[struct_data['k'] == k]
            if not k_data.empty:
                build_time = k_data['build_time_us'].values[0] / 1000.0  # ms
                k_label = f"K={k}" if k > 0 else "Text-based"
                print(f"  {k_label}:")
                print(f"    Build time: {build_time:.2f} ms")

                for _, row in k_data.iterrows():
                    print(f"    {row['query_type']:10s}: {row['mean_query_us']:8.2f} μs")

    print("\n" + "="*80)

def main():
    """Main function to generate all visualizations."""
    import sys

    # Check if CSV path provided as command-line argument
    csv_path = sys.argv[1] if len(sys.argv) > 1 else None

    print("Loading benchmark data...")
    df = load_benchmark_data(csv_path)

    print(f"Loaded {len(df)} benchmark results")
    print(f"Structures: {', '.join(sorted(df['structure'].unique()))}")
    print(f"K values: {', '.join(map(str, sorted(df['k'].unique())))}")
    print(f"Query types: {', '.join(sorted(df['query_type'].unique()))}")

    # Print summary table
    print_summary_table(df)

    # Create output directory
    output_dir = Path('benchmark_plots')
    output_dir.mkdir(exist_ok=True)

    # Generate individual query type plots
    print("\nGenerating query type plots...")
    for query_type in ['knn', 'range', 'search']:
        create_grouped_bar_chart(df, query_type,
                                output_dir / f'{query_type}_comparison.png')

    # Generate build time plot
    print("Generating build time plot...")
    create_build_time_chart(df, output_dir / 'build_time_comparison.png')

    # Generate comprehensive comparison
    print("Generating comprehensive comparison...")
    create_comprehensive_comparison(df, output_dir / 'comprehensive_comparison.png')

    print(f"\n✓ All plots saved to {output_dir}/")
    print("\nGenerated files:")
    print("  - knn_comparison.png")
    print("  - range_comparison.png")
    print("  - search_comparison.png")
    print("  - build_time_comparison.png")
    print("  - comprehensive_comparison.png")

    # Show plots
    print("\nDisplaying plots...")
    plt.show()

if __name__ == '__main__':
    main()
