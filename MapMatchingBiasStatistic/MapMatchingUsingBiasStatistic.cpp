#include "MapMatchingUsingBiasStatistic.h"

std::map<pair<int, int>, pair<double, double>> shortestDistPair2 = std::map<pair<int, int>, pair<double, double>>();

const double BETA_ARR[31] = {
	0,
	0.490376731, 0.82918373, 1.24364564, 1.67079581, 2.00719298,
	2.42513007, 2.81248831, 3.15745473, 3.52645392, 4.09511775,
	4.67319795, 5.41088180, 6.47666590, 6.29010734, 7.80752112,
	8.09074504, 8.08550528, 9.09405065, 11.09090603, 11.87752824,
	12.55107715, 15.82820829, 17.69496773, 18.07655652, 19.63438911,
	25.40832185, 23.76001877, 28.43289797, 32.21683062, 34.56991141
};

//��ͼƥ���������ݽṹ
struct Score//����ĳ���켣���Ӧ��һ����ѡ·��
{
	Edge* edge;//��ѡ·�ε�ָ��
	long double score;//��ѡ·�������е��������
	int preColumnIndex;//��ѡ·�ε�ǰ��·�ε�������
	double distLeft;//�켣���ͶӰ�㵽��ѡ·�����ľ���
	vector<long double> *priorProbs;
	Score(Edge* edge, long double score, int pre, double distLeft){
		this->edge = edge;
		this->score = score;
		this->preColumnIndex = pre;
		this->distLeft = distLeft;
		priorProbs = new vector<long double>();
	}
};

//������ʼ��㺯��
double EmissionProb2(double t, double dist){
	return exp(t*dist * dist * N2_SIGMAZ2) * SQR_2PI_SIGMAZ;
}

//ʹ�������·��ƫ�ý��е�ͼƥ��
//����ĳ���켣�����ڵ�����ѡ���������Ƶ������·����Ϊƥ��·�Σ������������û���κ�·��Ƶ���������ǰһƥ��·�ε��ù켣�㸽���ĺ�ѡ·��ת�Ƹ���������Ϊ�ù켣���ƥ��·��
list<Edge*> MapMatchingUsingBiasStatistic(list<GeoPoint*> &trajectory){
	long double BT = 34.56991141;
	list<Edge*> matchedEdges = list<Edge*>();
	GeoPoint* formerTrajPoint = NULL;
	Edge* formerMatchedEdge = NULL;
	for each (GeoPoint* trajPoint in trajectory)
	{
		Edge* matchedEdge = NULL;
		pair<int, int>gridCellIndex = routeNetwork.findGridCellIndex(trajPoint->lat, trajPoint->lon);
		if (biasSet.find(gridCellIndex) != biasSet.end()){
			int max = 0;
			for each (pair<Edge*, int> edgeCountPair in biasSet[gridCellIndex])
			{
				if (edgeCountPair.second > max){
					matchedEdge = edgeCountPair.first;
					max = edgeCountPair.second;
				}
			}
		}
		else{
			//���Ȼ�øù켣��ĺ�ѡ·�Σ�Ȼ��ѡ��ת�Ƹ������ĺ�ѡ·��
			vector<Edge*> canadidateEdges;//��ѡ·�μ���
			routeNetwork.getNearEdges(trajPoint->lat, trajPoint->lon, RANGEOFCANADIDATEEDGES, canadidateEdges);//���������ָ����Χ�ڵĺ�ѡ·�μ���
			if (formerTrajPoint != NULL){
				long double distBetweenTwoTrajPoints = GeoPoint::distM(trajPoint->lat, trajPoint->lon, formerTrajPoint->lat, formerTrajPoint->lon);
				double formerDistLeft = 0;
				routeNetwork.distMFromTransplantFromSRC(formerTrajPoint->lat, formerTrajPoint->lon, formerMatchedEdge, formerDistLeft);
				double deltaT = trajPoint->time - formerTrajPoint->time;
				long double maxProb = 0;
				for each (Edge* canadidateEdge in canadidateEdges)
				{
					double currentDistLeft = 0;
					routeNetwork.distMFromTransplantFromSRC(trajPoint->lat, trajPoint->lon, canadidateEdge, currentDistLeft);
					double formerDistToEnd = formerMatchedEdge->lengthM - formerDistLeft;//ǰһ���켣���ں�ѡ·���ϵ�ͶӰ���·���յ�ľ���
					list<Edge*> shortestPath = list<Edge*>();
					double routeNetworkDistBetweenTwoEdges = routeNetwork.shortestPathLength(formerMatchedEdge->endNodeId, canadidateEdge->startNodeId, shortestPath, currentDistLeft, formerDistToEnd, deltaT);
					long double routeNetworkDistBetweenTwoTrajPoints = routeNetworkDistBetweenTwoEdges + currentDistLeft + formerDistToEnd;
					long double transactionProb = exp(-fabs(distBetweenTwoTrajPoints - routeNetworkDistBetweenTwoTrajPoints) / BT) / BT;//ת�Ƹ���
					if (transactionProb > maxProb){
						matchedEdge = canadidateEdge;
					}
				}
			}
			else{
				long double maxProb = 0;
				for each (Edge* canadidateEdge in canadidateEdges)
				{
					double currentDistLeft = 0;
					double DistBetweenTrajPointAndEdge = routeNetwork.distMFromTransplantFromSRC(trajPoint->lat, trajPoint->lon, canadidateEdge, currentDistLeft);
					long double prob = EmissionProb2(1, DistBetweenTrajPointAndEdge);
					if (prob > maxProb){
						matchedEdge = canadidateEdge;
					}
				}
			}
		}
		matchedEdges.push_back(matchedEdge);
		formerTrajPoint = trajPoint;
		formerMatchedEdge = matchedEdge;
	}
	return matchedEdges;
}

//ʹ�������·��ƫ����Ϊ������ʽ��е�ͼƥ��
list<Edge*> MapMatchingUsingBiasStatisticAsPriorProb(list<GeoPoint*> &trajectory){
	list<Edge*> mapMatchingResult;//ȫ��ƥ��·��
	int sampleRate = (trajectory.size() > 1 ? (trajectory.back()->time - trajectory.front()->time) / (trajectory.size() - 1) : (trajectory.back()->time - trajectory.front()->time));//����켣ƽ��������
	if (sampleRate > 30){ sampleRate = 30; }
	long double BT = (long double)BETA_ARR[sampleRate];//���ݹ켣ƽ��������ȷ��betaֵ������ת�Ƹ���ʱʹ��
	vector<vector<Score>> scoreMatrix = vector<vector<Score>>();//���й켣��ĸ��ʾ���
	//��Ҫ��ÿ��ѭ������ǰ���µı���
	GeoPoint* formerTrajPoint = NULL;//��һ���켣�㣬����·������ʱ��Ҫ
	bool cutFlag = true;//û��ǰһ�켣���ǰһ�켣��û�д��ĺ�ѡ·��
	int currentTrajPointIndex = 0;//��ǰ�켣�������	
	for (list<GeoPoint*>::iterator trajectoryIterator = trajectory.begin(); trajectoryIterator != trajectory.end(); trajectoryIterator++)//����ÿ���켣��
	{
		double distBetweenTwoTrajPoints;//���켣����ֱ�Ӿ���
		double deltaT = -1;//��ǰ��켣�����ʱ��deltaT��ʾǰ�����켣����ʱ���
		if (formerTrajPoint != NULL){
			deltaT = (*trajectoryIterator)->time - formerTrajPoint->time;
			distBetweenTwoTrajPoints = GeoPoint::distM((*trajectoryIterator)->lat, (*trajectoryIterator)->lon, formerTrajPoint->lat, formerTrajPoint->lon);
		}
		vector<Score> scores = vector<Score>();//��ǰ�켣���Score����
		//������ʡ����������
		long double totalCount = 0;//��һ��
		pair<int, int>gridCellIndex = routeNetwork.findGridCellIndex((*trajectoryIterator)->lat, (*trajectoryIterator)->lon);
		if (biasSet.find(gridCellIndex) != biasSet.end()){
			for each (pair<Edge*, int> edgeCountPair in biasSet[gridCellIndex])
			{
				double currentDistLeft = 0;//��ǰ�켣���ں�ѡ·���ϵ�ͶӰ���·�����ľ���
				routeNetwork.distMFromTransplantFromSRC((*trajectoryIterator)->lat, (*trajectoryIterator)->lon, edgeCountPair.first, currentDistLeft);
				scores.push_back(Score(edgeCountPair.first, edgeCountPair.second, -1, currentDistLeft));
				totalCount += edgeCountPair.second;
			}
		}
		else
		{
			vector<Edge*> canadidateEdges = vector<Edge*>();//��ѡ·�μ���
			routeNetwork.getNearEdges((*trajectoryIterator)->lat, (*trajectoryIterator)->lon, RANGEOFCANADIDATEEDGES, canadidateEdges);//���������ָ����Χ�ڵĺ�ѡ·�μ���
			long double *emissionProbs = new long double[canadidateEdges.size()];//�����ѡ·�εķ������
			int currentCanadidateEdgeIndex = 0;//��ǰ��ѡ·�ε�����
			for each (Edge* canadidateEdge in canadidateEdges)
			{
				double currentDistLeft = 0;//��ǰ�켣���ں�ѡ·���ϵ�ͶӰ���·�����ľ���
				double DistBetweenTrajPointAndEdge = routeNetwork.distMFromTransplantFromSRC((*trajectoryIterator)->lat, (*trajectoryIterator)->lon, canadidateEdge, currentDistLeft);
				//������Щ��ѡ·�εķ������
				long double emissionProb = EmissionProb2(1, DistBetweenTrajPointAndEdge);
				scores.push_back(Score(canadidateEdge, emissionProb, -1, currentDistLeft));
				totalCount += emissionProb;
				currentCanadidateEdgeIndex++;
			}
		}
		for (size_t indexForScores = 0; indexForScores < scores.size(); indexForScores++){
			scores[indexForScores].score /= totalCount;
		}
		//������ʡ���ת�Ƹ���
		if (scoreMatrix.size()>0){
			for each (Score canadidateEdge in scores)
			{
				for (size_t index = 0; index < scoreMatrix.back().size(); index++){
					double formerDistLeft = scoreMatrix.back()[index].distLeft;//ǰһ���켣���ں�ѡ·���ϵ�ͶӰ���·�����ľ���
					double formerDistToEnd = scoreMatrix.back()[index].edge->lengthM - formerDistLeft;//ǰһ���켣���ں�ѡ·���ϵ�ͶӰ���·���յ�ľ���
					double routeNetworkDistBetweenTwoEdges;//��·������ľ���
					double routeNetworkDistBetweenTwoTrajPoints;//���켣���Ӧ��ͶӰ����·������
					if (canadidateEdge.edge == scoreMatrix.back()[index].edge){//���ǰһƥ��·�κ͵�ǰ��ѡ·����ͬһ·�Σ������߼���������ĲΪ·������
						routeNetworkDistBetweenTwoTrajPoints = fabs(canadidateEdge.distLeft - scoreMatrix.back()[index].distLeft);
					}
					else{
						pair<int, int> odPair = make_pair(scoreMatrix.back()[index].edge->endNodeId, canadidateEdge.edge->startNodeId);
						//���������յ����·�Ѿ���������Ҳ���INF
						if (shortestDistPair2.find(odPair) != shortestDistPair2.end() && shortestDistPair2[odPair].first < INF){
							//�����ǰdeltaT�µ��ƶ��������ޱ���̾���Ҫ�󣬵������·�����õ���Ҳ�Ǳ���ľ���ֵ����֮�õ��ľ���INF
							shortestDistPair2[odPair].first <= MAXSPEED*deltaT ? routeNetworkDistBetweenTwoEdges = shortestDistPair2[odPair].first : routeNetworkDistBetweenTwoEdges = INF;
						}
						else{
							if (shortestDistPair2.find(odPair) != shortestDistPair2.end() && deltaT <= shortestDistPair2[odPair].second){//����ĸ��������յ����·�����INF���ҵ�ǰdeltaT���ϴμ������·ʱ���ƶ�ʱ��ҪС��˵����ǰdeltaT�µõ������·��������INF
								routeNetworkDistBetweenTwoEdges = INF;
							}
							else{
								//����δ������������յ�����·��������ߵ�ǰdeltaT�ȱ����deltaTҪ�󣬿��ܵõ����������·�������֮����Ҫ���ú����������·
								list<Edge*> shortestPath = list<Edge*>();
								routeNetworkDistBetweenTwoEdges = routeNetwork.shortestPathLength(scoreMatrix.back()[index].edge->endNodeId, canadidateEdge.edge->startNodeId, shortestPath, canadidateEdge.distLeft, formerDistToEnd, deltaT);
								shortestDistPair2[odPair] = make_pair(routeNetworkDistBetweenTwoEdges, deltaT);
							}
						}
						routeNetworkDistBetweenTwoTrajPoints = routeNetworkDistBetweenTwoEdges + canadidateEdge.distLeft + formerDistToEnd;
					}
					long double transactionProb = exp(-fabs((long double)distBetweenTwoTrajPoints - (long double)routeNetworkDistBetweenTwoTrajPoints) / BT) / BT;//ת�Ƹ���
					cout << scoreMatrix.size() << " " << index << " " << scoreMatrix.back().size() << " " << scoreMatrix.back()[index].priorProbs->size() << endl;
					system("pause");
					scoreMatrix.back()[index].priorProbs->push_back(transactionProb);
					cout << "������" << endl;
				}
			}
		}
		cout << scores.size() << endl;
		cout << scores[10].priorProbs->size() << endl;
		scoreMatrix.push_back(scores);//�Ѹù켣���Scores�������scoreMatrix��
	}

	//�õ�ȫ��ƥ��·��
	int startColumnIndex;
	long double tmpMaxProb = 0;
	//�õ����һ���켣�����scoreMatrix��Ӧ���е÷���ߵ�����������ȫ��ƥ��·�����յ�
	for (size_t index = 0; index < scoreMatrix.back().size(); index++)
	{
		if (scoreMatrix.back()[index].score > tmpMaxProb){
			tmpMaxProb = scoreMatrix.back()[index].score;
			startColumnIndex = index;
		}
	}
	mapMatchingResult.push_front(scoreMatrix.back()[startColumnIndex].edge);
	int lastColumnIndex = startColumnIndex;
	for (int i = scoreMatrix.size() - 2; i >= 0; i--){
		long double maxPosteriorProb = -1;
		for (size_t j = 0; j < scoreMatrix[i].size(); j++){
			long double tmpPosteriorProb = scoreMatrix[i][j].score * scoreMatrix[i][j].priorProbs->at(lastColumnIndex) / scoreMatrix[i + 1][lastColumnIndex].score;
			if (tmpPosteriorProb > maxPosteriorProb){
				maxPosteriorProb = tmpPosteriorProb;
				startColumnIndex = j;
			}
		}
		mapMatchingResult.push_front(scoreMatrix[i][startColumnIndex].edge);
		lastColumnIndex = startColumnIndex;
	}
	return mapMatchingResult;
}