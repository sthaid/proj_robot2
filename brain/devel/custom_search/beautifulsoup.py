#!/bin/python3

import bs4
import requests
import sys

response = requests.get(sys.argv[1])

if response is not None:
    html = bs4.BeautifulSoup(response.text, 'html.parser')

    title = html.select("#firstHeading")[0].text
    print (title)

    paragraphs = html.select("p")
    for para in paragraphs:
        print (para.text)
