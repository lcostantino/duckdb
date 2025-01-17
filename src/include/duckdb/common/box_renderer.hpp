//===----------------------------------------------------------------------===//
//                         DuckDB
//
// duckdb/common/box_renderer.hpp
//
//
//===----------------------------------------------------------------------===//

#pragma once

#include "duckdb/common/constants.hpp"
#include "duckdb/common/vector.hpp"
#include "duckdb/main/query_profiler.hpp"
#include "duckdb/common/list.hpp"

namespace duckdb {
class ColumnDataCollection;
class ColumnDataRowCollection;

enum class ValueRenderAlignment { LEFT, MIDDLE, RIGHT };
enum class RenderMode : uint8_t { ROWS, COLUMNS };

enum class ResultRenderType { LAYOUT, COLUMN_NAME, COLUMN_TYPE, VALUE, NULL_VALUE, FOOTER };

class BaseResultRenderer {
public:
	BaseResultRenderer();
	virtual ~BaseResultRenderer();

	virtual void RenderLayout(const string &text) = 0;
	virtual void RenderColumnName(const string &text) = 0;
	virtual void RenderType(const string &text) = 0;
	virtual void RenderValue(const string &text, const LogicalType &type) = 0;
	virtual void RenderNull(const string &text, const LogicalType &type) = 0;
	virtual void RenderFooter(const string &text) = 0;

	BaseResultRenderer &operator<<(char c);
	BaseResultRenderer &operator<<(const string &val);

	void Render(ResultRenderType render_mode, const string &val);
	void SetValueType(const LogicalType &type);

private:
	LogicalType value_type;
};

class StringResultRenderer : public BaseResultRenderer {
public:
	void RenderLayout(const string &text) override;
	void RenderColumnName(const string &text) override;
	void RenderType(const string &text) override;
	void RenderValue(const string &text, const LogicalType &type) override;
	void RenderNull(const string &text, const LogicalType &type) override;
	void RenderFooter(const string &text) override;

	const string &str(); // NOLINT: mimic string stream

private:
	string result;
};

enum class LargeNumberRendering {
	NONE = 0,   // render all numbers as-is
	FOOTER = 1, // if there is a single row, adds a second footer row with a readable summarization of large numbers
	ALL = 2     // renders all large numbers
};

struct BoxRendererConfig {
	// a max_width of 0 means we default to the terminal width
	idx_t max_width = 0;
	// the maximum amount of rows to render
	idx_t max_rows = 20;
	// the limit that is applied prior to rendering
	// if we are rendering exactly "limit" rows then a question mark is rendered instead
	idx_t limit = 0;
	// the max col width determines the maximum size of a single column
	// note that the max col width is only used if the result does not fit on the screen
	idx_t max_col_width = 20;
	//! how to render NULL values
	string null_value = "NULL";
	//! Decimal separator (if any)
	char decimal_separator = '\0';
	//! Thousand separator (if any)
	char thousand_separator = '\0';
	//! Whether or not to render row-wise or column-wise
	RenderMode render_mode = RenderMode::ROWS;
	//! How to render large numbers
	LargeNumberRendering large_number_rendering = LargeNumberRendering::NONE;

#ifndef DUCKDB_ASCII_TREE_RENDERER
	const char *LTCORNER = "\342\224\214"; // NOLINT: "┌";
	const char *RTCORNER = "\342\224\220"; // NOLINT: "┐";
	const char *LDCORNER = "\342\224\224"; // NOLINT: "└";
	const char *RDCORNER = "\342\224\230"; // NOLINT: "┘";

	const char *MIDDLE = "\342\224\274";  // NOLINT: "┼";
	const char *TMIDDLE = "\342\224\254"; // NOLINT: "┬";
	const char *LMIDDLE = "\342\224\234"; // NOLINT: "├";
	const char *RMIDDLE = "\342\224\244"; // NOLINT: "┤";
	const char *DMIDDLE = "\342\224\264"; // NOLINT: "┴";

	const char *VERTICAL = "\342\224\202";   // NOLINT: "│";
	const char *HORIZONTAL = "\342\224\200"; // NOLINT: "─";

	const char *DOTDOTDOT = "\xE2\x80\xA6"; // NOLINT: "…";
	const char *DOT = "\xC2\xB7";           // NOLINT: "·";
	const idx_t DOTDOTDOT_LENGTH = 1;

#else
	// ASCII version
	const char *LTCORNER = "<";
	const char *RTCORNER = ">";
	const char *LDCORNER = "<";
	const char *RDCORNER = ">";

	const char *MIDDLE = "+";
	const char *TMIDDLE = "+";
	const char *LMIDDLE = "+";
	const char *RMIDDLE = "+";
	const char *DMIDDLE = "+";

	const char *VERTICAL = "|";
	const char *HORIZONTAL = "-";

	const char *DOTDOTDOT = "..."; // "...";
	const char *DOT = ".";         // ".";
	const idx_t DOTDOTDOT_LENGTH = 3;
#endif
};

class BoxRenderer {
	static const idx_t SPLIT_COLUMN;

public:
	explicit BoxRenderer(BoxRendererConfig config_p = BoxRendererConfig());

	string ToString(ClientContext &context, const vector<string> &names, const ColumnDataCollection &op);

	void Render(ClientContext &context, const vector<string> &names, const ColumnDataCollection &op,
	            BaseResultRenderer &ss);
	void Print(ClientContext &context, const vector<string> &names, const ColumnDataCollection &op);

private:
	//! The configuration used for rendering
	BoxRendererConfig config;

private:
	void RenderValue(BaseResultRenderer &ss, const string &value, idx_t column_width, ResultRenderType render_mode,
	                 ValueRenderAlignment alignment = ValueRenderAlignment::MIDDLE);
	string RenderType(const LogicalType &type);
	ValueRenderAlignment TypeAlignment(const LogicalType &type);
	string GetRenderValue(BaseResultRenderer &ss, ColumnDataRowCollection &rows, idx_t c, idx_t r,
	                      const LogicalType &type, ResultRenderType &render_mode);
	list<ColumnDataCollection> FetchRenderCollections(ClientContext &context, const ColumnDataCollection &result,
	                                                  idx_t top_rows, idx_t bottom_rows);
	list<ColumnDataCollection> PivotCollections(ClientContext &context, list<ColumnDataCollection> input,
	                                            vector<string> &column_names, vector<LogicalType> &result_types,
	                                            idx_t row_count);
	vector<idx_t> ComputeRenderWidths(const vector<string> &names, const vector<LogicalType> &result_types,
	                                  list<ColumnDataCollection> &collections, idx_t min_width, idx_t max_width,
	                                  vector<idx_t> &column_map, idx_t &total_length);
	void RenderHeader(const vector<string> &names, const vector<LogicalType> &result_types,
	                  const vector<idx_t> &column_map, const vector<idx_t> &widths, const vector<idx_t> &boundaries,
	                  idx_t total_length, bool has_results, BaseResultRenderer &renderer);
	void RenderValues(const list<ColumnDataCollection> &collections, const vector<idx_t> &column_map,
	                  const vector<idx_t> &widths, const vector<LogicalType> &result_types, BaseResultRenderer &ss);
	void RenderRowCount(string row_count_str, string shown_str, const string &column_count_str,
	                    const vector<idx_t> &boundaries, bool has_hidden_rows, bool has_hidden_columns,
	                    idx_t total_length, idx_t row_count, idx_t column_count, idx_t minimum_row_length,
	                    BaseResultRenderer &ss);

	string FormatNumber(const string &input);
	string ConvertRenderValue(const string &input, const LogicalType &type);
	string ConvertRenderValue(const string &input);
	//! Try to format a large number in a readable way (e.g. 1234567 -> 1.23 million)
	string TryFormatLargeNumber(const string &numeric);
};

} // namespace duckdb
