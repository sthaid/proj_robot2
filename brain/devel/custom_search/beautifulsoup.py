#!/bin/python3

import bs4
import requests
import sys

response = requests.get(sys.argv[1])

if response is not None:
    html = bs4.BeautifulSoup(response.text, 'html.parser')

    title = html.select("#firstHeading")[0].text
    print ("Title:", title)
    paragraphs = html.select("p")
    #for para in paragraphs:
    #    print (para.text)

    # just grab the text up to contents as stated in question
    #intro = '\n'.join([ para.text for para in paragraphs[0:5]])
    #print (intro)
    #print ("-----------------------------------------------")
    print("Paragraph:", paragraphs[0].text)
    #print ("-----------------------------------------------")
    print("Paragraph:", paragraphs[1].text)
    #print ("-----------------------------------------------")
    print("Paragraph:", paragraphs[2].text)
