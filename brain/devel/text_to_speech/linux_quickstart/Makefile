.PHONY: run clean

run: json_parse
	curl -X POST \
            -H "Authorization: Bearer "$(shell gcloud auth application-default print-access-token) \
            -H "Content-Type: application/json; charset=utf-8" \
            -d @request.json \
            https://texttospeech.googleapis.com/v1/text:synthesize > t2s.json
	json_parse < t2s.json > t2s.base64
	base64 t2s.base64 -d > t2s.mp3
	rm t2s.base64 t2s.json

json_parse: json_parse.c
	gcc -g -Wall -O2 -I. $^ -o $@ 

clean:
	rm -f json_parse t2s.base64 t2s.json t2s.mp3

