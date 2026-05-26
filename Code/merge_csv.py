import os
import glob
import pandas as pd

input_dir = r"C:\Users\jkopystianski\OneDrive - Infor\Desktop\PracaMgr\DFTA\OfficialResults\Elimination"
output_file = os.path.join(input_dir, "elimination_merged_results.csv")

csv_files = glob.glob(os.path.join(input_dir, "*.csv"))

if not csv_files:
    print("No CSV files found in the directory.")
else:
    dfs = []
    for file in csv_files:
        print(f"Reading: {os.path.basename(file)}")
        try:
            df = pd.read_csv(file)
            if df.empty:
                print(f"  -> Skipped (empty file)")
                continue
            dfs.append(df)
        except pd.errors.EmptyDataError:
            print(f"  -> Skipped (no data)")
            continue

    if not dfs:
        print("No data found in any CSV file.")
    else:
        merged = pd.concat(dfs, ignore_index=True)
        merged.to_csv(output_file, index=False)
        print(f"\nMerged {len(dfs)} files into: {output_file}")
        print(f"Total rows: {len(merged)}")
