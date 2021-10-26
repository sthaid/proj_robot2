TARGETS = livecaption synthesize_text

build: $(TARGETS)

synthesize_text: synthesize_text.go go.mod
	go build synthesize_text.go

livecaption: livecaption.go go.mod
	go build livecaption.go

go.mod:
	go mod init `basename ${PWD}`
	go get cloud.google.com/go/speech/apiv1
	go get cloud.google.com/go/texttospeech/apiv1

clean:
	rm -f $(TARGETS) go.mod go.sum

