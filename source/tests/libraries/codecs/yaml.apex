import codecs
import os

edge_yaml = "empty_val:\nnum_str: \"123\"\nbool_str: \"true\"\nplain_num: 42\nneg: -10"
edge_data = codecs.yaml_read(edge_yaml)

if type(edge_data) == "table"
    os.output("Empty val type: {type(edge_data.empty_val)}")
    os.output("Num string: {edge_data.num_str}")
    os.output("Bool string: {edge_data.bool_str}")
    os.output("Plain num: {string(edge_data.plain_num)}")
    os.output("Negative: {string(edge_data.neg)}")
else
    os.output("Failed to read edge case YAML")