// 2021-1 System Programming
// Thread programming homework
// A simple thread pipeline (multiple single producer and consumer version)
// Student Name : 박주훈(Park Juhun)
// Student Number : B511072
//
#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <assert.h>

struct autoPart {
	int partNumber;
	struct autoPart *next;
};

struct autoPartBox {
	int bid;	// autoPartBox id
	int SIZE;	// SIZE Of autoPartBox
	int count;  // the number of autoParts in the Box
	struct autoPart *lastPart;	// Pointer to the last auto part
	struct autoPart *firstPart; // Pointer to the first auto part
	pthread_mutex_t mutex;
	pthread_cond_t full;
	pthread_cond_t empty;
};

struct stageArg {
	int sid;
	int defectNumber;
};

#define ENDMARK -1
struct autoPartBox *AutoBox;
pthread_barrier_t barrier;

void sendAutoPart(int id, struct autoPart *ap, struct autoPartBox *apBox) {
	// produce 담당
	// autoPart 리스트 (next로 이어짐) 의 순서에 상관 없이, 이 함수로 들어온것만 receive 할 수 있도록 해야함.

//critical region start
	pthread_mutex_lock(&(apBox->mutex));

	// 꽉찼으면 기다리기
	while (apBox->count == apBox->SIZE){
		printf("SEND:: Stage %d thread waiting on autPartBox %d full \n",id,apBox->bid);
		pthread_cond_wait(&(apBox->full), &(apBox->mutex));
	}


	// 전송 == 박스 맨 뒤에 넣는것
	struct autoPart* temp = apBox->lastPart;
	apBox->lastPart = ap;	// 박스 맨뒤에 넣기
	printf("SEND:: Stage %d sending autoPart %d to autoPartBox %d\n",id,apBox->lastPart->partNumber,apBox->bid);

	if (apBox->firstPart == NULL){	// 박스가 비어있을땐(맨 처음일때) 박스 맨앞에 넣는것과 같다
		if (apBox->firstPart == NULL && apBox->count != 0){
			// 박스 맨앞이 비었는데 count가 0이 아닐경우 에러상황임
			// printf("\n\n\n!!!!!!!!!!!!something wrong!!!!!!!!!!!!\n\n\n");
		}
		apBox->firstPart = ap;
	} else { // 박스가 비어있지 않았다면 이전 last와 적절히 연결해 박스 자체적으로 리스트 형성
		temp->next = apBox->lastPart;
	}

	(apBox->count)++;
	// firstPart부터 lastPart까지 박스에 담겨있는 상황 (이 함수로 들어온 part들만)

	// empty였었으면 상대 깨우기
	if (apBox->count == 1){
		pthread_cond_signal(&(apBox->empty));
		printf("SEND:: Stage %d signals autoBoxPart %d NOT empty\n",id,apBox->bid);
	}

	pthread_mutex_unlock(&(apBox->mutex));
//critical region end

}

struct autoPart *receiveAutoPart(int id, struct autoPartBox *apBox) {
	// consume 담당
	// autoPart 리스트 (next로 이어짐) 의 순서에 상관 없이, send 함수로 들어갔던것만 여기서 receive 될 수 있도록 해야함.

//critical region start
	pthread_mutex_lock(&(apBox->mutex));
	
	// 비었으면 기다리기
	while (apBox->count == 0){
		printf("RECEIVE:: Stage %d waiting on autoPartBox %d empty\n",id,apBox->bid);
		pthread_cond_wait(&(apBox->empty), &(apBox->mutex));
	}

	// 수신 == 박스 맨 앞의것을 꺼내는것
	struct autoPart* autoPtr = apBox->firstPart;
	printf("RECEIVE:: Stage %d receiving autoPart %d from autoPartBox %d\n",id,autoPtr->partNumber,apBox->bid);

	if (apBox->count == 1){	// 원래 한개만있었을 경우
		apBox->firstPart = NULL;
	} else {	// 두개 이상 있었을경우
		apBox->firstPart = autoPtr->next;
	}
	(apBox->count)--;
	// firstPart부터 lastPart까지 박스에 담겨있는 상황 (send함수로 들어갔던 part들만)

	// full이었었으면 상대 깨우기
	if (apBox->count == (apBox->SIZE)-1){
		pthread_cond_signal(&(apBox->full));
		printf("RECEIVE:: Stage %d signals autoPartBox %d NOT full\n",id,apBox->bid);
	}

	pthread_mutex_unlock(&(apBox->mutex));	
//critical region end

	return(autoPtr);	// 수신한 autoPart 반환
}

// Generate autoParts and put the autoParts into the first autoPartBox
void *startThread(void *ag) {
	
	// printf("starting thread 0: start!\n");


	int nPart = *(int*)ag;
	int i;
	long sum=0;

	// 생산 (초기 파트 순서 구성)
	struct autoPart* partHead;
	struct autoPart* prev;
	for(i=0; i<nPart; i++){
		struct autoPart* temp = malloc(sizeof(struct autoPart));
		if (i == 0){
			partHead = temp;
		}
		if (i > 0){
			prev->next = temp;
		}
		temp->partNumber = abs(rand());

		prev = temp;
	}
	// (ENDMARK 생산)
	struct autoPart* temp = malloc(sizeof(struct autoPart));
	prev->next = temp;
	temp->partNumber = -1;
	temp->next = NULL;



	// 전송
	struct autoPart* autoPtr = partHead;
	while(autoPtr && autoPtr->partNumber != -1){
		printf("Start Thread Stage %d sending autoPart %d to autoPartBox %d\n",0,autoPtr->partNumber,0);
		sendAutoPart(0, autoPtr, &AutoBox[0]);

		sum += autoPtr->partNumber;	// 누적 합
		autoPtr = autoPtr->next;
	}
	// (ENDMARK 전송)
	printf("Start Thread Stage %d sending ENDMARK to autoPartBox %d\n",0,0);
	sendAutoPart(0, autoPtr, &AutoBox[0]);


	// 다보내면 exit하면서 부품번호 누적합 알려주기
	pthread_barrier_wait(&barrier);
	pthread_exit((void *)sum);
	
}

// Get autoParts from the last autoPartBox and add all of them
void *endThread(void *id) {
	int sid = *(int*)id;
	// printf("ending thread %d: start!\n", sid);
	
	long sum = 0;

	// receive
	// receive 반환값 이용해 부품번호 누적 더하기
	// 받은게 ENDMARK 였다면 exit하면서 부품번호 누적합 알려주기
	struct autoPart* autoPtr;
	while( (autoPtr = (receiveAutoPart(sid, &(AutoBox[sid-1]))))->partNumber != -1){
		printf("End Thread Stage %d receiving autoPart %d from autoPartBox %d\n", sid, autoPtr->partNumber, sid-1);
		sum += autoPtr->partNumber;
	}

	printf("End Thread Stage %d receiving ENDMARK from autoPartBox %d\n", sid, sid-1);

	// printf("ending thread %d bye~~\n", sid);
	pthread_barrier_wait(&barrier);
	pthread_exit((void *)sum);
}

// Check autoParts from the input box and remove faulty parts
// Add all faulty parts number; Put valid autoParts into the output box
// The faulty part number is a multiple of the stage defect number
void *stageThread(void *ptr) {
	
	int sid = ((struct stageArg*)ptr)->sid;
	int defectNumber = ((struct stageArg*)ptr)->defectNumber;
	long sum = 0;

	// printf("thread %d: start!\n", sid);

	// printf("thread %d 's defect number is %d\n", sid, defectNumber);


	// receive (from autoPartBox[sid-1])
	struct autoPart* autoPtr;
	while( (autoPtr = (receiveAutoPart(sid, &(AutoBox[sid-1]))))->partNumber != -1){
		printf("Stage %d receiving autoPart %d from autoPartBox %d\n", sid, autoPtr->partNumber, sid-1);

		// receive return값 이용해 part 불량검사
		// 불량검사 통과시 send (to autoPartBox[tid]), 미통과시 불량번호 누적 더하기
		if (autoPtr->partNumber % defectNumber == 0){	// 불량일 경우
			printf("Stage %d deleting autoPart %d\n", sid, autoPtr->partNumber);
			
			sum += autoPtr->partNumber;	// 불량번호 누적 합
			// 전송은 하지 않고 건너뛴다

		} else {	// 정상 부품일 경우엔 전송한다.
			printf("Stage %d sending autoPart %d to autoPartBox %d\n", sid, autoPtr->partNumber, sid);
			sendAutoPart(sid, autoPtr, &(AutoBox[sid]));
		}
	}

	// 받은게 ENDMARK 였다면 ENDMARK 전달하고, exit하면서 불량번호 누적합 알려주기
	printf("Stage %d receiving ENDMARK from autoPartBox %d\n", sid, sid-1);
	printf("Stage %d sending ENDMARK to autoPartBox %d\n", sid, sid);
	sendAutoPart(sid, autoPtr, &(AutoBox[sid]));

	// printf("thread %d bye~~\n", ((struct stageArg*)ptr)->sid );
	pthread_barrier_wait(&barrier);
	pthread_exit((void*)sum);

}



int main(int argc, char *argv[]) {

	long int startThreadSum, endThreadSum, stageThreadSum;
	int i; void *status;

	startThreadSum = endThreadSum = stageThreadSum = 0;
	srand(100);
	

	/* 초기화 */
	int nStages, BOXSIZE, nPart;
	nStages = atoi(argv[1]); // printf("nStages: %d\n", nStages);
	BOXSIZE = atoi(argv[2]); // printf("BOXSIZE: %d\n", BOXSIZE);
	nPart = atoi(argv[3]); // printf("nPart: %d\n", nPart);

	// autoPartBox 동적할당
	AutoBox = (struct autoPartBox*)malloc(sizeof(struct autoPartBox) * (nStages+1));
	for(i=0; i<nStages+1; i++){
		AutoBox[i].bid = i;
		AutoBox[i].SIZE = BOXSIZE;
		AutoBox[i].count = 0;
		AutoBox[i].lastPart = NULL;
		AutoBox[i].firstPart = NULL;
		pthread_mutex_init(&(AutoBox[i].mutex), NULL);
		pthread_cond_init(&(AutoBox[i].full), NULL);
		pthread_cond_init(&(AutoBox[i].empty), NULL);
	}
	// 박스들 동적할당 정상인지 확인
	// for(i=0; i<nStages+1; i++){
	// 	printf("autobox %d 's bid = %d\n", i, AutoBox[i].bid);
	// }


	// 생성 테스트
	struct stageArg stages[nStages];
	int startSID = 0, endSID = nStages+1;	// startSID 안쓰일 예정


	pthread_barrier_init(&barrier, NULL, nStages+3);


	// stageID 할당 및 각 stagethred 생성
	pthread_t startTid, endTid;
	pthread_t stageTid[nStages];
	pthread_create(&startTid, NULL, startThread, (void*) &nPart);
	for(i=0; i<nStages; i++){
		stages[i].sid = i+1;
		stages[i].defectNumber = atoi(argv[4+i]);
		pthread_create(&stageTid[i], NULL, stageThread, (void*) (void*)&stages[i]);
	}
	pthread_create(&endTid, NULL, endThread, (void*) &endSID);



	pthread_barrier_wait(&barrier);	// 기다려!


	// barrier sync 이후에 작동, 필수출력

	printf("*** Part Sum Information ***\n");
	pthread_join(startTid,&status); printf("startThread sum %ld\n", status);
	startThreadSum = (long)status;
	pthread_join(endTid,&status); printf("endThread sum %ld\n", status); 
	endThreadSum = (long)status;
	for(i=0; i < nStages; i++) {
		pthread_join(stageTid[i],&status); stageThreadSum += (long)status;
		printf("Stage %d sum %ld\n", i,status);
	}

	assert(startThreadSum == (endThreadSum+stageThreadSum));

	pthread_exit(0);

}
