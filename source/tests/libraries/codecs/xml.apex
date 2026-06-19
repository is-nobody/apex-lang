import codecs
import os

simple_xml = "<root><item id=\"1\">Apple</item><item id=\"2\">Banana</item></root>"
data_xml = codecs.xml_read(simple_xml)

if type(data_xml) == "table"
    os.output("XML Root Tag: {data_xml.__tag}")
    os.output("First Item Text: {data_xml.1.#text}")
    os.output("First Item ID: {data_xml.1.@id}")
else
    os.output("Failed to read XML")

xml_obj = (
    __tag = "config",
    "@version" = "1.0",
    1 = (
        __tag = "setting",
        "@name" = "theme",
        "#text" = "dark"
    ),
    2 = (
        __tag = "setting",
        "@name" = "lang",
        "#text" = "en"
    )
)
writed_xml = codecs.xml_write(xml_obj)
os.output("Generated XML:\n{writed_xml}")