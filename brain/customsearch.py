#!/usr/bin/python3

import sys
import os
import pprint
from googleapiclient.discovery import build

api_key = os.getenv("GOOGLE_CUSTOM_SEARCH_API_KEY")
search_engine_id = os.getenv("GOOGLE_CUSTOM_SEARCH_ENGINE_ID")
#print(api_key)
#print(search_engine_id)

service = build("customsearch", "v1", developerKey=api_key)

res = service.cse().list(
           q=sys.argv[1],
           cx=search_engine_id,
                ).execute()

pprint.pprint(res)

