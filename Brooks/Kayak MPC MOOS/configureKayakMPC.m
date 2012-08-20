% configureKayakMPC
% script for setting MPC and kayak model parameters
% used with kayak_MPC_KF AND MOOSkayakMPC
% calls generateTracklinesMPC

% changes
%{
- 8/14 changed system so that + rudder causes + heading (compass bearing),
added Bin
- 8/15/2012 - moved major settings here from generateTracklinesMPC
- 8/17/2012 - added kayak models
- 8/19/2012 - added packet loss params

%}

ifQuiet = 1;          % if cvx is run in quiet mode
uDelay = 1;
plotStep = 0;
% packet loss probability:
probPLoss = .02;
loss2MPC = 1;

%% PARAMETERS

% System Params
%n = 3, m=1;
% KASSANDRA OFFSET (no modem, altimeter, 8/16/2012)
%rOff=-7;
rOff = 3;
trueNorthAdjustment = -15;

%syss='heading';
%syss='crossTrack';
syss = 'crossTrack_CLheading';

%kayak = 'kassandra_modem_smallR';
kayak = 'kassandra_modem_30R';
%kayak = 'nostromo_modem';

% Planning horizon (steps)
T = 10;
% T = 6;
% T = 15;

% Time step (sec)
%dt = 1;
dt = 4;
%dt = 6;

% for gen matrices for KF @ 2hz
%dt = 0.5;

%tracklineType='straight';
tracklineType = 'pavilion_1turn';
%tracklineType = 'hexagon';

switch tracklineType
    case 'straight'
        %Nsec=30;
        Nsec=60;
        ox = 20;oy = -30;
        % bearing of straight line:
        desB = deg2rad(80);
    case 'pavilion_1turn'
        secPerLeg = ceil(120/dt)*dt;
        %secPerLeg = ceil(60/dt)*dt;
        numLegs=2;
        Nsec = secPerLeg*numLegs;
        ox = 50;oy = -20;
        pavAngOffset = -20;
        kinkAng = deg2rad(30);
    case 'hexagon'
        numLegs = 6;
        secPerLeg = 50;
        Nsec = secPerLeg*numLegs;
        ox = 20;
        oy = -30;
end
N = ceil(Nsec/dt);  % total sim steps

switch syss
    case 'crossTrack'
        n = 4;  % STATES
        m = 1;  % CONTROL INPUT
        q = 2;  % MEASUREMENTS
        % SIM initial state (ACTUAL BEARING): (IN PHYSICAL UNITS)
        % [headingAccel(deg/s^2), headingRate(deg/s), heading(deg), crossTrack (m)]
        x0c = [0;0;73+20;0];
        
        % max/mins (IN PHYSICAL UNITS)
        %xmax= [20 20 90 2*x0c(4)]'.*ones(n,1);xmin=-xmax;
        xmax = [30 30 90 100]'.*ones(n,1);xmin=-xmax;
        umax = 20*ones(m,1); umin = -umax;
        
    case 'crossTrack_CLheading'
        n = 3;  % STATES
        m = 1;  % CONTROL INPUT
        q = 1;  % MEASUREMENTS
        
        % SIM initial state (ACTUAL BEARING): (IN PHYSICAL UNITS)
        % [headingRate(deg/s), heading(deg), crossTrack (m)]
        x0c = [0;73+20;0];
        
        % max/mins (IN PHYSICAL UNITS)
        %xmax= [20 90 2*x0c(4)]'.*ones(n,1);xmin=-xmax;
        xmax = [30 90 100]'.*ones(n,1);xmin=-xmax;
        % u is setpt for desired heading error
        umax = 30*ones(m,1); umin = -umax;
        
end

% MPC params
mu=10;              % sparse control weight
%T=ceil(N/2);        % horizon length
% (T set above)

Qmpc = eye(n);         % state cost
%Rmpc = eye(m);         % control cost
% maybe scale by P from Ricatti??
Pmpc = 10*eye(n);     % terminal state cost

% Qmpc and Pmpc are scaled by CdAll below (
% (set weights based on physical units)
Qmpc(n,n) = 2;
Pmpc(n,n) = 10*2;
%Qmpc(n,n) = .05;
%Pmpc(n,n) = .05;

% number of 'continuous-time' samples in one time step
nc=dt/(1e-1);

% System params
% kayak cross-track model
switch kayak
    case 'nostromo_modem'
        
        Krate=1/1.56; % rudder in to heading rate out
        wn=sqrt(1.56);
        zeta=1.01/(2*wn);
        speed=2; % m/s
        
    case 'kassandra_modem_smallR'
        
        Krate = 1.37/1.13;
        wn = sqrt(1.13);
        zeta = 0.5/(2*wn);
        speed = 0.8;
        
    case 'kassandra_modem_30R'
        
        Krate = 1.19/1.13;
        wn = sqrt(1.13);
        zeta = 1.09/(2*wn);
        speed = 0.8;    % with 65% thrust...?
        
end

%slew rate
slewRate=(10)*dt; % rad/s

% constrain perpendicular speed to be fraction of speed*dt
% (half: max 30 deg heading diff) Taylor 1st order |error|=0.0236
% (1/3: max 20 deg heading diff) Taylor 1st order |error|=0.007
angle2speed = 1/2;
%angle2speed=1;

% KF params
Bnoise=eye(n);  % process noise input gain
% simple cross-track only:
%Qcross=1e-2;    % continuous time PSD
%Qheading=1e-2;
Qcross = 5;
Qheading = 5;
Qkfc=zeros(n);
Qkfc(n-1,n-1)=Qheading;
Qkfc(n,n)=Qcross;
%Qkfc=[0 0 0 0;0 0 0 0;0 0 Qheading 0;0 0 0 Qcross];
Qkfd = Qkfc/dt;
% noise in heading and cross-track (correlation...)?

% % measurement noise:
% RCompass=3;       % compass std dev.
% RGPS=5;           % GPS std dev.
% measurement noise:
RCompass=.5;       % compass std dev.
RGPS=3;           % GPS std dev.
switch syss
    case 'crossTrack'
        Rkf=[RCompass 0;0 RGPS];
    case 'crossTrack_CLheading'
        Rkf=[RGPS];
end

%% compute desired trajectory
% (in generateTracklinesMPC)
generateTracklinesMPC

%% generate A, B matrices

s = tf('s');
K=wn^2*Krate;
headingRate = (K/(s^2 + s + wn^2));
heading=headingRate/s;
% dz/dt = U*sin(theta) ~ U*theta (in radians)
Kcross=speed*pi/180;

switch syss
    case 'crossTrack'
        
        crossTrack = heading*Kcross/s;
        sysC=crossTrack;
        
        % set up system
        [num den] = tfdata(sysC);
        % [num den] = tfdata(sys*exp(-s*sysDelay));
        
        [Ac Bc Cc Dc] = tf2ss(num{1},den{1});
        Cc = [1 0 0 0;0 K 0 0;0 0 1 0;Cc];
        
        % signs and input for desired heading
        Ac(4,3) = -1;
        Bin = [0 0 0 0;0 0 0 0;0 0 0 0;0 0 1 0];
        
        sysCss=ss(Ac,Bc,Cc,Dc);
        sysd = c2d(sysCss,dt);
        
        [Ad Bd CdAll Dd] = ssdata(sysd);    % this uses full state output
        Cd = [0,0,CdAll(3,3),0;0,0,0,CdAll(4,4)];   % this for MEASUREMENT
        
        % repeat to get noise input
        sysdNoise = c2d(ss(Ac,Bnoise,Cc,Dc),dt);
        [~,Bdnoise,~,~] = ssdata(sysdNoise);
        
        % repeat to get setpoint input
        sysdSetpt = c2d(ss(Ac,Bin,Cc,Dc),dt);
        [~,Bdin,~,~] = ssdata(sysdSetpt);
        
    case 'crossTrack_CLheading'
        
        wnH = 1;
        zetaH = .7;
        
        % this version just uses stable 2nd order for heading
        CLheading = wnH^2/(s^2+2*wnH*zetaH*s+wnH^2);
        crossTrack = CLheading*Kcross/s;
        
        %%%%%%%%%%%%%%%%%%%%%
        sysC = crossTrack;
        % set up system
        [num den] = tfdata(sysC);
        
        [Ac Bc Cc Dc] = tf2ss(num{1},den{1});
        Cc = [1 0 0 ;0 1 0 ;Cc];
        
        % Bc is input matrix for PSI (setpt for CL heading)
        % Bin is input matrix for desired bearing of trackline
        Ac(3,2) = -1;
        Bin = [0 0 0;0 0 0;0 1 0];
        
        sysCss=ss(Ac,Bc,Cc,Dc);
        sysd = c2d(sysCss,dt);
        
        [Ad Bd CdAll Dd] = ssdata(sysd);    % this uses full state output
        Cd = [0,0,CdAll(3,3)];   % this for MEASUREMENT
        
        % repeat to get noise input
        sysdNoise = c2d(ss(Ac,Bnoise,Cc,Dc),dt);
        [~,Bdnoise,~,~] = ssdata(sysdNoise);
        
        % repeat to get setpoint input
        sysdSetpt = c2d(ss(Ac,Bin,Cc,Dc),dt);
        [~,Bdin,~,~] = ssdata(sysdSetpt);
        
        
end

% convert z, max/min, Qmpc to discrete time states
x0 = CdAll\x0c;
xmax = CdAll\xmax; xmin = CdAll\xmin;
Qmpc = Qmpc.*CdAll;
Pmpc = Pmpc.*CdAll;
Qhalf = sqrtm(Qmpc);

% setup structures to pass to KF, MPC
sys=struct('n',n,'m',m,'Ad',Ad,'Bd',Bd,'Cd',Cd,'Bdnoise',Bdnoise,...
    'dt',dt,'Bdin',Bdin);
MPCparams=struct('Qhalf',Qhalf,'Pmpc',Pmpc,'mu',mu,'T',T,...
    'ifQuiet',ifQuiet,...
    'speed',speed,'angle2speed',angle2speed,'slewRate',slewRate,...
    'xmax',xmax,'xmin',xmin,'umax',umax,'umin',umin,'CdAll',CdAll);
KFparams=struct('Rkf',Rkf,'Qkfd',Qkfd);

%% plot step responses
if(plotStep)
    tmax=dt*5;stepin=15;  % deg, input
    [y t1] = step(headingRate,tmax);[y,t2] =step(heading,max(t1));
    [y,tc]= step(crossTrack,max(t1));[y_d,td] = step(sysd,max(tc));
    figure
    colorvec={'g--.','b--.','r--.','k--.'};
    for i = 1:n
        stairs(td,stepin*y_d(:,i),colorvec{i})
        xlabel('sec');hold on
    end
    legend('heading accel','heading rate','heading','cross-track')
    figure;subplot(1,2,1)
    step(sysCss*15,tmax)
    hold on;step(sysd*15,tmax);subplot(1,2,2);step(sysd*15,tmax)
end