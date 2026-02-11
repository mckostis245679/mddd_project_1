#!/usr/bin/env python3
"""
Movie Search GUI - Tkinter Desktop Interface
Alternative desktop GUI for hybrid tree + LSH movie search
"""

import tkinter as tk
from tkinter import ttk, scrolledtext, messagebox
import subprocess
import re
from typing import List, Dict, Tuple
import threading

# Field definitions matching C++ implementation
NUMERIC_FIELDS = {
    1: "budget",
    2: "revenue",
    3: "runtime",
    4: "popularity",
    5: "vote_average",
    6: "vote_count",
    7: "release_year"
}

STRING_FIELDS = {
    1: "title",
    2: "genre_names",
    3: "production_company_names",
    4: "original_language",
    5: "origin_country"
}

TREE_TYPES = {
    "KD-Tree": 1,
    "R-Tree": 2,
    "QuadTree": 3,
    "2D Range Tree": 4
}


class MovieSearchGUI:
    def __init__(self, root):
        self.root = root
        self.root.title("Movie Search - Hybrid Tree + LSH")
        self.root.geometry("1000x700")

        # Variables
        self.tree_var = tk.StringVar(value="KD-Tree")
        self.dim_var = tk.IntVar(value=2)
        self.n_var = tk.IntVar(value=5)
        self.search_mode_var = tk.StringVar(value="Hybrid")
        self.lsh_query_var = tk.StringVar(value="Action Adventure")

        self.numeric_vars = {}
        self.string_vars = {}
        self.range_entries = {}

        self.create_widgets()

    def create_widgets(self):
        # Main container
        main_frame = ttk.Frame(self.root, padding="10")
        main_frame.grid(row=0, column=0, sticky=(tk.W, tk.E, tk.N, tk.S))

        self.root.columnconfigure(0, weight=1)
        self.root.rowconfigure(0, weight=1)
        main_frame.columnconfigure(1, weight=1)

        # Left panel - Configuration
        config_frame = ttk.LabelFrame(main_frame, text="Configuration", padding="10")
        config_frame.grid(row=0, column=0, sticky=(tk.W, tk.E, tk.N, tk.S), padx=(0, 10))

        row = 0

        # Tree selection
        ttk.Label(config_frame, text="Tree Structure:").grid(row=row, column=0, sticky=tk.W, pady=5)
        tree_combo = ttk.Combobox(config_frame, textvariable=self.tree_var, values=list(TREE_TYPES.keys()), state="readonly")
        tree_combo.grid(row=row, column=1, sticky=(tk.W, tk.E), pady=5)
        tree_combo.bind("<<ComboboxSelected>>", self.on_tree_changed)
        row += 1

        # Dimensions
        ttk.Label(config_frame, text="Dimensions:").grid(row=row, column=0, sticky=tk.W, pady=5)
        self.dim_spinbox = ttk.Spinbox(config_frame, from_=2, to=5, textvariable=self.dim_var, width=10)
        self.dim_spinbox.grid(row=row, column=1, sticky=tk.W, pady=5)
        self.dim_spinbox.config(command=self.on_dimension_changed)
        row += 1

        # Numeric fields
        ttk.Label(config_frame, text="Numeric Fields:", font=("", 10, "bold")).grid(row=row, column=0, columnspan=2, sticky=tk.W, pady=(10, 5))
        row += 1

        self.numeric_frame = ttk.Frame(config_frame)
        self.numeric_frame.grid(row=row, column=0, columnspan=2, sticky=(tk.W, tk.E), pady=5)
        row += 1

        for field_id, field_name in NUMERIC_FIELDS.items():
            var = tk.BooleanVar(value=False)
            self.numeric_vars[field_name] = var

        self.update_numeric_checkboxes()

        # Range bounds frame
        ttk.Label(config_frame, text="Range Bounds:", font=("", 10, "bold")).grid(row=row, column=0, columnspan=2, sticky=tk.W, pady=(10, 5))
        row += 1

        self.range_frame = ttk.Frame(config_frame)
        self.range_frame.grid(row=row, column=0, columnspan=2, sticky=(tk.W, tk.E), pady=5)
        row += 1

        # String fields
        ttk.Label(config_frame, text="String Fields (LSH):", font=("", 10, "bold")).grid(row=row, column=0, columnspan=2, sticky=tk.W, pady=(10, 5))
        row += 1

        for field_id, field_name in STRING_FIELDS.items():
            var = tk.BooleanVar(value=(field_name in ["genre_names", "production_company_names"]))
            self.string_vars[field_name] = var
            cb = ttk.Checkbutton(config_frame, text=field_name, variable=var)
            cb.grid(row=row, column=0, columnspan=2, sticky=tk.W)
            row += 1

        # LSH Query
        ttk.Label(config_frame, text="LSH Query:").grid(row=row, column=0, sticky=tk.W, pady=(10, 5))
        row += 1
        lsh_entry = ttk.Entry(config_frame, textvariable=self.lsh_query_var)
        lsh_entry.grid(row=row, column=0, columnspan=2, sticky=(tk.W, tk.E), pady=5)
        row += 1

        # Top-N
        ttk.Label(config_frame, text="Top-N Results:").grid(row=row, column=0, sticky=tk.W, pady=5)
        n_spinbox = ttk.Spinbox(config_frame, from_=1, to=100, textvariable=self.n_var, width=10)
        n_spinbox.grid(row=row, column=1, sticky=tk.W, pady=5)
        row += 1

        # Search mode
        ttk.Label(config_frame, text="Search Mode:", font=("", 10, "bold")).grid(row=row, column=0, columnspan=2, sticky=tk.W, pady=(10, 5))
        row += 1

        mode_frame = ttk.Frame(config_frame)
        mode_frame.grid(row=row, column=0, columnspan=2, sticky=tk.W, pady=5)

        ttk.Radiobutton(mode_frame, text="Hybrid (Range + LSH)", variable=self.search_mode_var, value="Hybrid").pack(anchor=tk.W)
        ttk.Radiobutton(mode_frame, text="Range Only", variable=self.search_mode_var, value="Range").pack(anchor=tk.W)
        ttk.Radiobutton(mode_frame, text="LSH Only", variable=self.search_mode_var, value="LSH").pack(anchor=tk.W)
        row += 1

        # Search button
        self.search_btn = ttk.Button(config_frame, text="🔍 Search", command=self.on_search)
        self.search_btn.grid(row=row, column=0, columnspan=2, sticky=(tk.W, tk.E), pady=10)

        # Right panel - Results
        results_frame = ttk.LabelFrame(main_frame, text="Results", padding="10")
        results_frame.grid(row=0, column=1, sticky=(tk.W, tk.E, tk.N, tk.S))
        results_frame.columnconfigure(0, weight=1)
        results_frame.rowconfigure(1, weight=1)

        # Stats frame
        self.stats_label = ttk.Label(results_frame, text="Ready to search...", relief=tk.SUNKEN, padding=5)
        self.stats_label.grid(row=0, column=0, sticky=(tk.W, tk.E), pady=(0, 10))

        # Results text
        self.results_text = scrolledtext.ScrolledText(results_frame, width=60, height=30, wrap=tk.WORD)
        self.results_text.grid(row=1, column=0, sticky=(tk.W, tk.E, tk.N, tk.S))

        # Configure tags for formatting
        self.results_text.tag_config("title", font=("", 11, "bold"), foreground="#2c3e50")
        self.results_text.tag_config("info", foreground="#34495e")
        self.results_text.tag_config("score", foreground="#27ae60", font=("", 10, "bold"))

        # Initialize
        self.on_tree_changed()

    def update_numeric_checkboxes(self):
        # Clear existing checkboxes
        for widget in self.numeric_frame.winfo_children():
            widget.destroy()

        # Create new checkboxes
        row = 0
        for field_name, var in self.numeric_vars.items():
            cb = ttk.Checkbutton(self.numeric_frame, text=field_name, variable=var, command=self.on_numeric_field_changed)
            cb.grid(row=row, column=0, sticky=tk.W)
            row += 1

    def update_range_inputs(self):
        # Clear existing range inputs
        for widget in self.range_frame.winfo_children():
            widget.destroy()
        self.range_entries.clear()

        # Get selected numeric fields
        selected = [name for name, var in self.numeric_vars.items() if var.get()]

        if not selected:
            ttk.Label(self.range_frame, text="Select numeric fields first", foreground="gray").grid(row=0, column=0)
            return

        # Create range inputs for selected fields
        for i, field_name in enumerate(selected):
            ttk.Label(self.range_frame, text=f"{field_name}:").grid(row=i, column=0, sticky=tk.W, pady=2)

            lower_entry = ttk.Entry(self.range_frame, width=10)
            lower_entry.grid(row=i, column=1, padx=2)
            lower_entry.insert(0, "0")

            ttk.Label(self.range_frame, text="to").grid(row=i, column=2, padx=2)

            upper_entry = ttk.Entry(self.range_frame, width=10)
            upper_entry.grid(row=i, column=3, padx=2)
            upper_entry.insert(0, "100")

            self.range_entries[field_name] = (lower_entry, upper_entry)

    def on_tree_changed(self, event=None):
        tree_name = self.tree_var.get()
        if tree_name in ["QuadTree", "2D Range Tree"]:
            self.dim_var.set(2)
            self.dim_spinbox.config(state="disabled")
        else:
            self.dim_spinbox.config(state="normal")
        self.on_dimension_changed()

    def on_dimension_changed(self):
        dims = self.dim_var.get()
        # Auto-select first N fields
        for i, (name, var) in enumerate(self.numeric_vars.items()):
            var.set(i < dims)
        self.update_range_inputs()

    def on_numeric_field_changed(self):
        self.update_range_inputs()

    def generate_cpp_input(self, config: Dict) -> str:
        """Generate input string for C++ program"""
        lines = []

        # Tree type
        lines.append(str(config['tree_type']))

        # Dimensions (only if KD or R-Tree)
        if config['tree_type'] in [1, 2]:
            lines.append(str(config['dimensions']))

        # Numeric field IDs
        numeric_ids = [str(k) for k, v in NUMERIC_FIELDS.items() if v in config['numeric_fields']]
        lines.append(','.join(numeric_ids))

        # Range bounds for each numeric field
        for field in config['numeric_fields']:
            lower, upper = config['ranges'][field]
            lines.append(str(lower))
            lines.append(str(upper))

        # String field IDs for LSH
        string_ids = [str(k) for k, v in STRING_FIELDS.items() if v in config['string_fields']]
        lines.append(','.join(string_ids))

        # LSH query
        lines.append(config['lsh_query'])

        # N (top-N results)
        lines.append(str(config['n']))

        # Search mode: 'h' = hybrid, 'r' = range only, 'l' = LSH only
        search_mode = config.get('search_mode', 'Hybrid')
        if search_mode == 'Range':
            lines.append('r')
        elif search_mode == 'LSH':
            lines.append('l')
        else:
            lines.append('h')  # Hybrid (default)

        return '\n'.join(lines) + '\n'

    def parse_cpp_output(self, output: str) -> Tuple[Dict, str]:
        """Parse C++ program output"""
        stats = {}

        # Extract statistics
        tree_match = re.search(r'Tree candidates:\s*(\d+)', output)
        lsh_match = re.search(r'LSH candidates\s*:\s*(\d+)', output)
        intersection_match = re.search(r'Intersection top-\d+:\s*(\d+)', output)

        if tree_match:
            stats['tree_candidates'] = int(tree_match.group(1))
        if lsh_match:
            stats['lsh_candidates'] = int(lsh_match.group(1))
        if intersection_match:
            stats['intersection_count'] = int(intersection_match.group(1))

        # Extract results section
        results_section = ""
        if "RESULTS" in output:
            parts = output.split("RESULTS")
            if len(parts) > 1:
                results_section = parts[1].split("Done.")[0].strip()

        return stats, results_section

    def run_search(self, config: Dict):
        """Execute C++ search program with given configuration"""
        input_str = self.generate_cpp_input(config)
        search_mode = config.get('search_mode', 'Hybrid')

        try:
            process = subprocess.Popen(
                ['./executable'],
                stdin=subprocess.PIPE,
                stdout=subprocess.PIPE,
                stderr=subprocess.PIPE,
                text=True,
                cwd='/home/chris/uni/mddd_project_1'
            )

            stdout, stderr = process.communicate(input=input_str, timeout=120)

            if process.returncode != 0:
                self.display_error(f"Error: {stderr}")
                return

            stats, results = self.parse_cpp_output(stdout)
            stats['mode'] = search_mode
            self.display_results(stats, results)

        except subprocess.TimeoutExpired:
            self.display_error("Error: Search timed out (120s)")
        except Exception as e:
            self.display_error(f"Error: {str(e)}")
        finally:
            self.search_btn.config(state="normal")

    def on_search(self):
        # Validate inputs
        selected_numeric = [name for name, var in self.numeric_vars.items() if var.get()]
        dims = self.dim_var.get()

        if len(selected_numeric) != dims:
            messagebox.showerror("Error", f"Please select exactly {dims} numeric field(s)")
            return

        selected_string = [name for name, var in self.string_vars.items() if var.get()]
        if not selected_string:
            messagebox.showerror("Error", "Please select at least one string field")
            return

        # Validate ranges
        ranges = {}
        try:
            for field_name, (lower_entry, upper_entry) in self.range_entries.items():
                lower = float(lower_entry.get())
                upper = float(upper_entry.get())
                ranges[field_name] = (min(lower, upper), max(lower, upper))
        except ValueError:
            messagebox.showerror("Error", "Invalid range values. Please enter numbers.")
            return

        # Build config
        config = {
            'tree_type': TREE_TYPES[self.tree_var.get()],
            'dimensions': dims,
            'numeric_fields': selected_numeric,
            'ranges': ranges,
            'string_fields': selected_string,
            'lsh_query': self.lsh_query_var.get(),
            'n': self.n_var.get(),
            'search_mode': self.search_mode_var.get()
        }

        # Clear results
        self.results_text.delete(1.0, tk.END)
        self.stats_label.config(text="Searching...")
        self.search_btn.config(state="disabled")

        # Run search in background thread
        thread = threading.Thread(target=self.run_search, args=(config,))
        thread.daemon = True
        thread.start()

    def display_results(self, stats: Dict, results: str):
        """Display search results"""
        # Update stats
        mode = stats.get('mode', 'Hybrid')
        stats_text = f"Mode: {mode} | Tree: {stats.get('tree_candidates', 'N/A')} | LSH: {stats.get('lsh_candidates', 'N/A')} | Final: {stats.get('intersection_count', 'N/A')}"
        self.stats_label.config(text=stats_text)

        # Display results
        self.results_text.delete(1.0, tk.END)

        if results:
            # Parse and format results
            lines = results.split('\n')
            for line in lines:
                if not line.strip():
                    continue

                if line.strip().startswith(tuple('0123456789')):
                    # Result line
                    parts = line.split('|')
                    if len(parts) >= 2:
                        # Extract title and rank
                        rank_title = parts[0].strip()
                        self.results_text.insert(tk.END, f"{rank_title}\n", "title")

                        # Other info
                        info = ' | '.join(p.strip() for p in parts[1:])
                        self.results_text.insert(tk.END, f"  {info}\n", "info")
                        self.results_text.insert(tk.END, "\n")
                else:
                    self.results_text.insert(tk.END, f"{line}\n")
        else:
            self.results_text.insert(tk.END, "No results found.\n")

    def display_error(self, message: str):
        """Display error message"""
        self.stats_label.config(text="Error occurred")
        self.results_text.delete(1.0, tk.END)
        self.results_text.insert(tk.END, message, "info")
        messagebox.showerror("Error", message)


def main():
    root = tk.Tk()
    app = MovieSearchGUI(root)
    root.mainloop()


if __name__ == "__main__":
    main()
