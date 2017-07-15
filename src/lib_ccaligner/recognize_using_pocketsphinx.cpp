/*
 * Author   : Saurabh Shrivastava
 * Email    : saurabh.shrivastava54@gmail.com
 * Link     : https://github.com/saurabhshri
*/

#include "recognize_using_pocketsphinx.h"


Aligner::Aligner(std::string inputAudioFileName, std::string inputSubtitleFileName)
{
    _audioFileName = inputAudioFileName;
    _subtitleFileName = inputSubtitleFileName;

    WaveFileData * file = new WaveFileData(_audioFileName);
    file->read();
    _samples = file->getSamples();

    SubtitleParserFactory *subParserFactory = new SubtitleParserFactory(_subtitleFileName);
    SubtitleParser *parser = subParserFactory->getParser();
    _subtitles = parser->getSubtitles();

    //delete file;
//    delete parser;
//    delete subParserFactory;
}

bool Aligner::generateGrammar()
{
    generate(_subtitles, all);
}

bool Aligner::initDecoder(std::string modelPath, std::string lmPath, std::string dictPath, std::string fsgPath, std::string logPath)
{
    /*
    _config = cmd_ln_init(NULL,
                          ps_args(),TRUE,
                          "-hmm", "model/",
                          "-lm", "elon.lm",
                          //"-fsg","tempFiles/fsg/3560.fsg",
                          "-dict","elon-good.dict",
                          "-logfn", "mylog.log",
                          NULL);
     */

    _modelPath = modelPath;
    _lmPath = lmPath;
    _dictPath = dictPath;
    _fsgPath = fsgPath;
    _logPath = logPath;

    _config = cmd_ln_init(NULL,
                          ps_args(),TRUE,
                          "-hmm", modelPath.c_str(),
                          "-lm", lmPath.c_str(),
                          "-dict",dictPath.c_str(),
                          "-cmn", "batch",
                          "-logfn", logPath.c_str(),
//                          "-lw", "1.0",
//                          "-beam", "1e-80",
//                          "-wbeam", "1e-60",
//                          "-pbeam", "1e-80",
                          NULL);

    if (_config == NULL) {
        fprintf(stderr, "Failed to create config object, see log for  details\n");
        return -1;
    }

    _ps = ps_init(_config);

    if (_ps == NULL) {
        fprintf(stderr, "Failed to create recognizer, see log for  details\n");
        return -1;
    }
}

bool Aligner::printWordTimes(cmd_ln_t *config, ps_decoder_t *ps)
{
    ps_start_stream(ps);
    int frame_rate = cmd_ln_int32_r(config, "-frate");
    ps_seg_t *iter = ps_seg_iter(ps);
    while (iter != NULL) {
        int32 sf, ef, pprob;
        float conf;

        ps_seg_frames(iter, &sf, &ef);
        pprob = ps_seg_prob(iter, NULL, NULL, NULL);
        conf = logmath_exp(ps_get_logmath(ps), pprob);
        printf(">>> %s \t %.3f \t %.3f\n", ps_seg_word(iter), ((float)sf / frame_rate),
               ((float) ef / frame_rate));
        iter = ps_seg_next(iter);
    }
}

bool Aligner::align()
{
    for(SubtitleItem *sub : _subtitles)
    {


        long int dialogueStartsAt = sub->getStartTime();
        long int dialogueLastsFor = (sub->getEndTime() - dialogueStartsAt);

        long int samplesAlreadyRead = dialogueStartsAt * 16;
        long int samplesToBeRead = dialogueLastsFor * 16;

        /*
         * 00:00:19,320 --> 00:00:21,056
         * Why are you boring?
         *
         * dialogueStartsAt : 19320 ms
         * dialogueEndsAt   : 21056 ms
         * dialogueLastsFor : 1736 ms
         *
         * SamplesAlreadyRead = 19320 ms * 16 samples/ms = 309120 samples
         * SampleToBeRead     = 1736  ms * 16 samples/ms = 27776 samples
         *
         */

        const int16_t * sample = _samples.data();

        _rv = ps_start_utt(_ps);
        _rv = ps_process_raw(_ps, sample + samplesAlreadyRead, samplesToBeRead, FALSE, FALSE);
        _rv = ps_end_utt(_ps);

        _hyp = ps_get_hyp(_ps, &_score);

        std::cout<<"\n\n-----------------------------------------\n\n";
        std::cout<<"Start time of dialogue : "<<dialogueStartsAt<<"\n";
        std::cout<<"End time of dialogue   : "<<sub->getEndTime()<<"\n\n";
        std::cout<<"Recognised  : "<<_hyp<<"\n";
        std::cout<<"Actual      : "<<sub->getDialogue()<<"\n\n";

        printWordTimes(_config, _ps);
        std::cout<<"\n\n-----------------------------------------\n\n";

    }
}

bool Aligner::reInitDecoder(cmd_ln_t *config, ps_decoder_t *ps)
{
    ps_reinit(_ps, _config);
}

bool Aligner::alignWithFSG()
{
    for(SubtitleItem *sub : _subtitles)
    {
        if(sub->getDialogue().empty())
            continue;

        long int dialogueStartsAt = sub->getStartTime();
        std::string fsgname(_fsgPath + std::to_string(dialogueStartsAt));
        fsgname += ".fsg";

        cmd_ln_t *subConfig;
        subConfig = cmd_ln_init(NULL,
                                ps_args(),TRUE,
                                "-hmm", _modelPath.c_str(),
                                "-lm", _lmPath.c_str(),
                                "-dict",_dictPath.c_str(),
                                "-logfn", _logPath.c_str(),
                                "-fsg", fsgname.c_str(),
//                          "-lw", "1.0",
//                          "-beam", "1e-80",
//                          "-wbeam", "1e-60",
//                          "-pbeam", "1e-80",
                                NULL);

            if (subConfig == NULL) {
                fprintf(stderr, "Failed to create config object, see log for  details\n");
                return -1;
            }


            ps_reinit(_ps,subConfig);

            if (_ps == NULL) {
                fprintf(stderr, "Failed to create recognizer, see log for  details\n");
                return -1;
            }


        long int dialogueLastsFor = (sub->getEndTime() - dialogueStartsAt);

        long int samplesAlreadyRead = dialogueStartsAt * 16;
        long int samplesToBeRead = dialogueLastsFor * 16;

        const int16_t * sample = _samples.data();

        _rv = ps_start_utt(_ps);
        _rv = ps_process_raw(_ps, sample + samplesAlreadyRead, samplesToBeRead, FALSE, FALSE);
        _rv = ps_end_utt(_ps);

        _hyp = ps_get_hyp(_ps, &_score);

        std::cout<<"\n\n-----------------------------------------\n\n";
        std::cout<<"Start time of dialogue : "<<dialogueStartsAt<<"\n";
        std::cout<<"End time of dialogue   : "<<sub->getEndTime()<<"\n\n";
        std::cout<<"Recognised  : "<<_hyp<<"\n";
        std::cout<<"Actual      : "<<sub->getDialogue()<<"\n\n";

        printWordTimes(subConfig, _ps);
        std::cout<<"\n\n-----------------------------------------\n\n";


        cmd_ln_free_r(subConfig);

    }
}

Aligner::~Aligner()
{
    //std::system("rm -rf tempFiles/");
    ps_free(_ps);
    cmd_ln_free_r(_config);
}