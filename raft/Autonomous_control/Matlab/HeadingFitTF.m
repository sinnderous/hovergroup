%function to fit model to heading rate data

close all
%Load Data from Tests
count = 100;
%--------
load('HeadingTestThrust250.mat')
theta0 = rad2deg(x(3,20));
headingvec = rad2deg(x(3,1:count))-theta0;
 window = [sin(pi/6) sin(pi/3), sin(pi/2) sin(pi/3) sin(pi/6)];
 c = conv(headingvec,window,'same')/sum(window); 
headingratevec250 = diff(c)./diff(t(1:size(c,2)));
tvec250 = t(20:30)-t(20);
%--------
load('HeadingTestThrust200.mat')
theta0 = rad2deg(x(3,20));
headingvec = rad2deg(x(3,1:count))-theta0;
 window = [sin(pi/6) sin(pi/3), sin(pi/2) sin(pi/3) sin(pi/6)];
 c = conv(headingvec,window,'same')/sum(window); 
headingratevec200 = diff(c)./diff(t(1:size(c,2)));
tvec200 = t(20:30)-t(20);
%--------
load('HeadingTestThrust150b.mat')
theta0 = rad2deg(x(3,20));
headingvec = rad2deg(x(3,1:count))-theta0;
 window = [sin(pi/6) sin(pi/3), sin(pi/2) sin(pi/3) sin(pi/6)];
 c = conv(headingvec,window,'same')/sum(window); 
headingratevec150 = diff(c)./diff(t(1:size(c,2)));
tvec150 = t(20:30)-t(20);
%--------
load('HeadingTestThrust100b.mat')
theta0 = rad2deg(x(3,20));
headingvec = rad2deg(x(3,1:count))-theta0;
 window = [sin(pi/6) sin(pi/3), sin(pi/2) sin(pi/3) sin(pi/6)];
 c = conv(headingvec,window,'same')/sum(window); 
headingratevec100 = diff(c)./diff(t(1:size(c,2)));
tvec100 = t(20:30)-t(20);
%--------
%Calculate Model
s=tf([1 0],1);
%second-order system
a=0.095;
b=5;
K = 400*a*b;
sys = K/((s+a)*(s+b));
[ys,ts] = step(sys,1);

%first order system
% a=1;
% K=50*a;
% sys = K/(s+a);
% [ys,ts] = step(sys,4);

figure
plot(tvec200,headingratevec200(20:30),'--g.',ts,ys*0.8,'-g',tvec150,headingratevec150(20:30),'--b.',tvec100,headingratevec100(20:30)+1,'--m.','LineWidth',3);
xlabel('Time [s]','FontSize',16)
ylabel('Heading Rate [deg/sec]','FontSize',16);
set(gca,'FontSize',16)
axis([0 1 -5 25]);
h0=legend('Heading Rate Data','Model');
rect = [0.35, 0.8, 0.05, 0.05];
set(h0,'Position',rect);
grid on
hold on
plot(ts,ys*0.6,'-b',ts,ys*0.4,'-m','LineWidth',3)

print(gcf, '-depsc','RaftModel1')
print(gcf, '-dpng','RaftModel1')
print(gcf, '-djpeg','RaftModel1')