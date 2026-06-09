import csv
import json
import sys
from copy import copy
from openpyxl import load_workbook
from openpyxl.utils import range_boundaries
from openpyxl.utils.cell import coordinate_from_string, column_index_from_string
from openpyxl.styles import Alignment

def copy_cell(source_cell, target_cell):
    """Copy cell value, style, formatting, etc."""
    target_cell.value = source_cell.value
    if source_cell.has_style:
        target_cell.font = copy(source_cell.font)
        target_cell.border = copy(source_cell.border)
        target_cell.fill = copy(source_cell.fill)
        target_cell.number_format = copy(source_cell.number_format)
        target_cell.protection = copy(source_cell.protection)
        target_cell.alignment = copy(source_cell.alignment)

def inject_csv_into_template(csv_file, template_file, output_file, mapping_file, dry_run=False):
    # Load CSV (first row = headers)
    with open(csv_file, newline='', encoding='utf-8') as f:
        reader = csv.DictReader(f)
        rows = list(reader)

    if not rows:
        print("❌ CSV file is empty.")
        return

    # Load mapping
    with open(mapping_file, "r", encoding="utf-8") as f:
        mapping = json.load(f)

    sheet_name = mapping["sheet_name"]
    cell_map = mapping["columns"]

    # Determine template height (max row from all mappings)
    template_height = 0
    for excel_cell in cell_map.values():
        if ":" in excel_cell:
            min_col, min_row, max_col, max_row = range_boundaries(excel_cell)
            template_height = max(template_height, max_row)
        else:
            col_letter, row_number = coordinate_from_string(excel_cell)
            template_height = max(template_height, row_number)
    
    print(f"📏 Template height detected: {template_height} rows")

    # Load Excel template
    wb = load_workbook(template_file)
    ws = wb[sheet_name]

    # Determine the template column range (find max column used)
    template_max_col = 0
    for excel_cell in cell_map.values():
        if ":" in excel_cell:
            min_col, min_row, max_col, max_row = range_boundaries(excel_cell)
            template_max_col = max(template_max_col, max_col)
        else:
            col_letter, row_number = coordinate_from_string(excel_cell)
            col_index = column_index_from_string(col_letter)
            template_max_col = max(template_max_col, col_index)
    
    print(f"📐 Template width detected: {template_max_col} columns")

    # Extract cell ranges from mapping that need to be merged
    mapping_ranges = []
    for csv_col, excel_cell in cell_map.items():
        if ":" in excel_cell:
            min_col, min_row, max_col, max_row = range_boundaries(excel_cell)
            mapping_ranges.append((csv_col, min_col, min_row, max_col, max_row))
    
    print(f"📦 Found {len(mapping_ranges)} cell ranges to merge from mapping")

    # Process all CSV rows into one Excel file
    for idx, record in enumerate(rows, start=0):
        print(f"\n▶ Processing CSV row {idx+1}: {record}")
        
        row_offset = idx * (template_height + 1)  # +1 for spacing between entries
        
        if not dry_run and idx > 0:
            # Copy the entire template block for this CSV row
            print(f"  📋 Copying template to rows {row_offset + 1}-{row_offset + template_height}")
            
            for row in range(1, template_height + 1):
                for col in range(1, template_max_col + 1):
                    source_cell = ws.cell(row=row, column=col)
                    target_cell = ws.cell(row=row + row_offset, column=col)
                    copy_cell(source_cell, target_cell)

        # Now fill in the values from CSV and merge cells according to mapping
        for csv_col, excel_cell in cell_map.items():
            if csv_col not in record:
                print(f"  ⚠ CSV column '{csv_col}' not found in row {idx+1}. Skipping.")
                continue

            value = record[csv_col]

            if ":" in excel_cell:  # it's a range
                min_col, min_row, max_col, max_row = range_boundaries(excel_cell)

                top_left_row = min_row + row_offset
                top_left_col = min_col
                bottom_right_row = max_row + row_offset
                bottom_right_col = max_col

                target_range = f"{ws.cell(row=top_left_row, column=top_left_col).coordinate}:{ws.cell(row=bottom_right_row, column=bottom_right_col).coordinate}"

                if dry_run:
                    print(f"  Dry-run: would insert '{value}' into {ws.cell(row=top_left_row, column=top_left_col).coordinate} and merge {target_range}")
                else:
                    # Write value to top-left cell
                    cell = ws.cell(row=top_left_row, column=top_left_col)
                    cell.value = value
                    
                    # Apply justified alignment and word wrap
                    cell.alignment = Alignment(horizontal='justify', vertical='top', wrap_text=True)
                    
                    # Merge the range
                    ws.merge_cells(target_range)
                    print(f"  ✓ Wrote '{value}' to {cell.coordinate} and merged {target_range}")
            else:  # single cell
                col_letter, row_number = coordinate_from_string(excel_cell)
                col_index = column_index_from_string(col_letter)
                target_row = row_number + row_offset

                if dry_run:
                    print(f"  Dry-run: would insert '{value}' into {col_letter}{target_row}")
                else:
                    cell = ws.cell(row=target_row, column=col_index)
                    cell.value = value
                    
                    # Apply justified alignment and word wrap
                    cell.alignment = Alignment(horizontal='justify', vertical='top', wrap_text=True)
                    
                    print(f"  ✓ Wrote '{value}' to {col_letter}{target_row}")

    if not dry_run:
        wb.save(output_file)
        print(f"\n✅ All {len(rows)} CSV rows injected into {output_file}")

if __name__ == "__main__":
    if len(sys.argv) < 5:
        print("Usage: python inject_csv_to_template.py <csv_file> <template_file> <output_file> <mapping_file> [--dry-run]")
        sys.exit(1)

    csv_file, template_file, output_file, mapping_file = sys.argv[1:5]
    dry_run = "--dry-run" in sys.argv

    inject_csv_into_template(csv_file, template_file, output_file, mapping_file, dry_run=dry_run)